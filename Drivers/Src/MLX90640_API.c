/**
 * @copyright (C) 2017 Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "MLX90640_I2C_Driver.h"
#include "MLX90640_API.h"

extern I2C_HandleTypeDef *i2cHandle;

typedef enum {
    MLX_STATE_CHECK_READY,
    MLX_STATE_READ_PIXELS,
    MLX_STATE_READ_AUX,
    MLX_STATE_READ_CTRL,
    MLX_STATE_DONE
} MLX_DMA_State_t;

static volatile MLX_DMA_State_t mlxState = MLX_STATE_CHECK_READY;
static volatile uint8_t mlxI2C_Busy = 0;
static uint16_t ttempStatus;
static uint16_t ttempAux[64];
static uint16_t ttempCtrl;


static void ExtractVDDParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractPTATParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractGainParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractTgcParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractResolutionParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractKsTaParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractKsToParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractAlphaParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractOffsetParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractKtaPixelParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractKvPixelParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractCPParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static void ExtractCILCParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);
static int ExtractDeviatingPixels(uint16_t *eeData, paramsMLX90640 *mlx90640);
static int CheckAdjacentPixels(uint16_t pix1, uint16_t pix2);
static float GetMedian(float *values, int n);
static int IsPixelBad(uint16_t pixel,paramsMLX90640 *params);
static int ValidateFrameData(uint16_t *frameData);
static int ValidateAuxData(uint16_t *auxData);



static uint32_t writeNoVerifyFailCount = 0;
/*
 * added 5/15/2026
 * MLX90640_I2CWriteNoVerify(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
 * Writes a single 16-bit word to a sensor register without reading back to
 * verify. Use this instead of MLX90640_I2CWrite() when you need a fast write
 * on a time-sensitive path and don't need confirmation the value was accepted.
 *
 * In practice this is only used to reset the status register (0x8000) inside
 * MLX90640_StepFrameDMA() between frames. That register doesn't need
 * verification — if the write fails the next frame read will simply return
 * not-ready and retry, so a failed write is self-correcting.
 *
 * Returns HAL_OK (0) on success, non-zero on I2C failure.
 */
int MLX90640_I2CWriteNoVerify(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
    uint8_t pData[2];
    HAL_StatusTypeDef result;
    pData[0] = (uint8_t)((data >> 8) & 0xFF);
    pData[1] = (uint8_t)(data & 0xFF);
    result = HAL_I2C_Mem_Write(i2cHandle, (slaveAddr << 1), writeAddress,
        I2C_MEMADD_SIZE_16BIT, pData, sizeof(pData), 10);
    if (result != HAL_OK)  writeNoVerifyFailCount++;
    return result;
}

/*
 * MLX90640_DumpEE(uint8_t slaveAddr, uint16_t *eeData)
 * Reads all 832 words of factory calibration data from the sensor's internal
 * EEPROM into the provided buffer.
 *
 * Call this ONCE at startup before anything else. The raw data is not usable
 * directly — pass it to MLX90640_ExtractParameters() to parse it into a
 * structured form.
 *
 * 'eeData' must point to a buffer of at least 832 uint16_t values.
 * Returns 0 on success, negative on I2C error.
 */
int MLX90640_DumpEE(uint8_t slaveAddr, uint16_t *eeData)
{
     return MLX90640_I2CRead(slaveAddr, MLX90640_EEPROM_START_ADDRESS, MLX90640_EEPROM_DUMP_NUM, eeData);
}

/*
 * MLX90640_SynchFrame(uint8_t slaveAddr)
 * Synchronizes the STM32 with the sensor's internal frame timing by waiting
 * until the sensor signals that new data is ready, then clears that flag.
 *
 * Use this if you need precise timing control — for example, to ensure you
 * always read a freshly completed frame rather than one that's mid-capture.
 * In our main loop we don't call this directly; GetFrameData() handles the
 * data-ready polling internally.
 *
 * Returns 0 on success, negative on I2C error.
 */
int MLX90640_SynchFrame(uint8_t slaveAddr)
{
    uint16_t dataReady = 0;
    uint16_t statusRegister;
    int error = 1;

    error = MLX90640_I2CWrite(slaveAddr, MLX90640_STATUS_REG, MLX90640_INIT_STATUS_VALUE);
    if(error == -MLX90640_I2C_NACK_ERROR)
    {
        return error;
    }

    while(dataReady == 0)
    {
        error = MLX90640_I2CRead(slaveAddr, MLX90640_STATUS_REG, 1, &statusRegister);
        if(error != MLX90640_NO_ERROR)
        {
            return error;
        }
        //dataReady = statusRegister & 0x0008;
        dataReady = MLX90640_GET_DATA_READY(statusRegister);
    }

   return MLX90640_NO_ERROR;
}



/*
 * MLX90640_TriggerMeasurement(uint8_t slaveAddr)
 * Manually triggers a single measurement cycle on the sensor.
 * Only needed when the sensor is configured in step mode (triggered/on-demand
 * capture) rather than continuous mode. In our setup the sensor runs
 * continuously, so this is not called.
 *
 * Returns 0 on success, negative on error.
 */
int MLX90640_TriggerMeasurement(uint8_t slaveAddr)
{
    int error = 1;
    uint16_t ctrlReg;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &ctrlReg);

    if ( error != MLX90640_NO_ERROR)
    {
        return error;
    }

    ctrlReg |= MLX90640_CTRL_TRIG_READY_MASK;
    error = MLX90640_I2CWrite(slaveAddr, MLX90640_CTRL_REG, ctrlReg);

    if ( error != MLX90640_NO_ERROR)
    {
        return error;
    }

    error = MLX90640_I2CGeneralReset();

    if ( error != MLX90640_NO_ERROR)
    {
        return error;
    }

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &ctrlReg);

    if ( error != MLX90640_NO_ERROR)
    {
        return error;
    }

    if ((ctrlReg & MLX90640_CTRL_TRIG_READY_MASK) != 0)
    {
        return -MLX90640_MEAS_TRIGGER_ERROR;
    }

    return MLX90640_NO_ERROR;
}

uint8_t MLX90640_HandleMemRxCplt(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != i2cHandle) {
        return 0;
    }

    mlxI2C_Busy = 0;
    return 1;
}

void MLX90640_ResetDMAState(void) {
    mlxState = MLX_STATE_CHECK_READY;
    mlxI2C_Busy = 0;
}

int MLX90640_StepFrameDMA(uint8_t slaveAddr, uint16_t *frameData) {
    if (mlxI2C_Busy) return 0; // Still waiting for the current DMA transfer

    switch (mlxState) {
        case MLX_STATE_CHECK_READY:
            // 1. Start reading status register
            if (MLX90640_I2CRead_DMA(slaveAddr, 0x8000, 1, &ttempStatus) != 0) {
                mlxI2C_Busy = 0;
                mlxState = MLX_STATE_CHECK_READY;
                return 0;
            }
            mlxI2C_Busy = 1;
            mlxState = MLX_STATE_READ_PIXELS;
            break;

        case MLX_STATE_READ_PIXELS:
            // tempStatus was just filled by DMA. Swap it!
            ttempStatus = __REV16(ttempStatus);

            if (!(ttempStatus & 0x0008)) { // Data not ready
                mlxState = MLX_STATE_CHECK_READY; // Loop back
                return 0;
            }

            // Reset status reg using no-verify write to avoid blocking on readback
            MLX90640_I2CWriteNoVerify(slaveAddr, 0x8000, 0x0030);

            // 2. Start massive Pixel Read
            if (MLX90640_I2CRead_DMA(slaveAddr, 0x0400, 768, frameData) != 0) {
                mlxI2C_Busy = 0;
                mlxState = MLX_STATE_CHECK_READY;
                return 0;
            }
            mlxI2C_Busy = 1;
            mlxState = MLX_STATE_READ_AUX;
            break;

        case MLX_STATE_READ_AUX:
            // Pixels are in! Start reading Aux data
            if (MLX90640_I2CRead_DMA(slaveAddr, 0x0700, 64, ttempAux) != 0) {
                mlxI2C_Busy = 0;
                mlxState = MLX_STATE_CHECK_READY;
                return 0;
            }
            mlxI2C_Busy = 1;
            mlxState = MLX_STATE_READ_CTRL;
            break;

        case MLX_STATE_READ_CTRL:
            // Aux is in! Start reading Control Reg
            if (MLX90640_I2CRead_DMA(slaveAddr, 0x800D, 1, &ttempCtrl) != 0) {
                mlxI2C_Busy = 0;
                mlxState = MLX_STATE_CHECK_READY;
                return 0;
            }
            mlxI2C_Busy = 1;
            mlxState = MLX_STATE_DONE;
            break;

        case MLX_STATE_DONE:
            // Everything is in RAM. Now do the final assembly and byte swaps.
            ttempCtrl = __REV16(ttempCtrl);

            // Swap the 768 pixels
            for(int i=0; i<768; i++) frameData[i] = __REV16(frameData[i]);

            // Swap and move Aux data
            for(int i=0; i<64; i++) frameData[768+i] = __REV16(ttempAux[i]);

            frameData[832] = ttempCtrl;
            frameData[833] = ttempStatus & 0x0001; // Subpage bit

            mlxState = MLX_STATE_CHECK_READY; // Reset for next frame
            return 1; // SIGNAL: Frame is completely ready to be drawn!

        default:
            mlxState = MLX_STATE_CHECK_READY;
            break;
    }
    return 0; // Frame not finished yet
}

/*
 * MLX90640_GetFrameData(uint8_t slaveAddr, uint16_t *frameData)
 * Waits for the sensor to finish capturing a subpage, then reads the raw pixel
 * data and auxiliary data (used for ambient temp, voltage, gain) into frameData.
 *
 * The MLX90640 captures in two alternating subpages (a checkerboard pattern).
 * Each call to this function reads ONE subpage. Call it TWICE per full frame —
 * once for subpage 0, once for subpage 1 — to populate all 768 pixels.
 *
 * 'frameData' must point to a buffer of at least 834 uint16_t values:
 *   [0–767]   raw pixel ADC values
 *   [768–831] auxiliary data (temperatures, gain, voltage reference)
 *   [832]     control register snapshot
 *   [833]     which subpage this frame belongs to (0 or 1)
 *
 * Returns the subpage number (0 or 1) on success, negative on error.
 * Pass this buffer directly to MLX90640_CalculateTo() to get temperatures.
 */
int MLX90640_GetFrameData(uint8_t slaveAddr, uint16_t *frameData)
{
    uint16_t dataReady = 0;
    uint16_t controlRegister1;
    uint16_t statusRegister;
    int error = 1;
    uint16_t data[64];
    uint8_t cnt = 0;

    while(dataReady == 0)
    {
        error = MLX90640_I2CRead(slaveAddr, MLX90640_STATUS_REG, 1, &statusRegister);
        if(error != MLX90640_NO_ERROR)
        {
            return error;
        }
        //dataReady = statusRegister & 0x0008;
        dataReady = MLX90640_GET_DATA_READY(statusRegister);
    }

    error = MLX90640_I2CWrite(slaveAddr, MLX90640_STATUS_REG, MLX90640_INIT_STATUS_VALUE);
    if(error == -MLX90640_I2C_NACK_ERROR)
    {
        return error;
    }

    error = MLX90640_I2CRead(slaveAddr, MLX90640_PIXEL_DATA_START_ADDRESS, MLX90640_PIXEL_NUM, frameData);
    if(error != MLX90640_NO_ERROR)
    {
        return error;
    }

    error = MLX90640_I2CRead(slaveAddr, MLX90640_AUX_DATA_START_ADDRESS, MLX90640_AUX_NUM, data);
    if(error != MLX90640_NO_ERROR)
    {
        return error;
    }

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);
    frameData[832] = controlRegister1;
    //frameData[833] = statusRegister & 0x0001;
    frameData[833] = MLX90640_GET_FRAME(statusRegister);

    if(error != MLX90640_NO_ERROR)
    {
        return error;
    }

    error = ValidateAuxData(data);
    if(error == MLX90640_NO_ERROR)
    {
        for(cnt=0; cnt<MLX90640_AUX_NUM; cnt++)
        {
            frameData[cnt+MLX90640_PIXEL_NUM] = data[cnt];
        }
    }

    error = ValidateFrameData(frameData);
    if (error != MLX90640_NO_ERROR)
    {
        return error;
    }

    return frameData[833];
}

static int ValidateFrameData(uint16_t *frameData)
{
    uint8_t line = 0;

    for(int i=0; i<MLX90640_PIXEL_NUM; i+=MLX90640_LINE_SIZE)
    {
        if((frameData[i] == 0x7FFF) && (line%2 == frameData[833])) return -MLX90640_FRAME_DATA_ERROR;
        line = line + 1;
    }

    return MLX90640_NO_ERROR;
}

static int ValidateAuxData(uint16_t *auxData)
{

    if(auxData[0] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;

    for(int i=8; i<19; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    for(int i=20; i<23; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    for(int i=24; i<33; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    for(int i=40; i<51; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    for(int i=52; i<55; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    for(int i=56; i<64; i++)
    {
        if(auxData[i] == 0x7FFF) return -MLX90640_FRAME_DATA_ERROR;
    }

    return MLX90640_NO_ERROR;

}

/*
 * MLX90640_ExtractParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
 * Parses the raw EEPROM dump (from MLX90640_DumpEE) and fills the
 * paramsMLX90640 struct with organized calibration values ready for use.
 *
 * Call this ONCE at startup, immediately after MLX90640_DumpEE().
 * After this, the eeData buffer is no longer needed and can be reused.
 * The resulting mlxParams struct is passed to CalculateTo() every frame.
 *
 * Returns 0 on success, or a negative error code if too many bad/broken
 * pixels are detected (which would indicate a damaged sensor).
 */
int MLX90640_ExtractParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int error = 0;

    ExtractVDDParameters(eeData, mlx90640);
    ExtractPTATParameters(eeData, mlx90640);
    ExtractGainParameters(eeData, mlx90640);
    ExtractTgcParameters(eeData, mlx90640);
    ExtractResolutionParameters(eeData, mlx90640);
    ExtractKsTaParameters(eeData, mlx90640);
    ExtractKsToParameters(eeData, mlx90640);
    ExtractCPParameters(eeData, mlx90640);
    ExtractAlphaParameters(eeData, mlx90640);
    ExtractOffsetParameters(eeData, mlx90640);
    ExtractKtaPixelParameters(eeData, mlx90640);
    ExtractKvPixelParameters(eeData, mlx90640);
    ExtractCILCParameters(eeData, mlx90640);
    error = ExtractDeviatingPixels(eeData, mlx90640);

    return error;

}

//------------------------------------------------------------------------------

/*
 * MLX90640_SetResolution(uint8_t slaveAddr, uint8_t resolution)
 * Sets the ADC resolution used inside the sensor for pixel measurements.
 * Higher resolution = more precise raw values, but slower conversion time.
 *
 * Valid values:
 *   0x00 = 16-bit
 *   0x01 = 17-bit
 *   0x02 = 18-bit (default)
 *   0x03 = 19-bit
 *
 * Returns 0 on success, negative on error.
 */
int MLX90640_SetResolution(uint8_t slaveAddr, uint8_t resolution)
{
    uint16_t controlRegister1;
    uint16_t value;
    int error;

    //value = (resolution & 0x03) << 10;
    value = ((uint16_t)resolution << MLX90640_CTRL_RESOLUTION_SHIFT);
    value &= ~MLX90640_CTRL_RESOLUTION_MASK;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);

    if(error == MLX90640_NO_ERROR)
    {
        value = (controlRegister1 & MLX90640_CTRL_RESOLUTION_MASK) | value;
        error = MLX90640_I2CWrite(slaveAddr, MLX90640_CTRL_REG, value);
    }

    return error;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetCurResolution(uint8_t slaveAddr)
 * Reads the currently active ADC resolution setting from the sensor.
 * Returns the resolution code (0–3, see SetResolution for meaning),
 * or a negative value on I2C error.
 */
int MLX90640_GetCurResolution(uint8_t slaveAddr)
{
    uint16_t controlRegister1;
    int resolutionRAM;
    int error;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);
    if(error != MLX90640_NO_ERROR)
    {
        return error;
    }
    resolutionRAM = (controlRegister1 & ~MLX90640_CTRL_RESOLUTION_MASK) >> MLX90640_CTRL_RESOLUTION_SHIFT;

    return resolutionRAM;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_SetRefreshRate(uint8_t slaveAddr, uint8_t refreshRate)
 * Sets how many frames per second the sensor captures.
 * We use 0x04 (4 Hz) in main.c — a good balance of speed and noise.
 *
 * Valid values:
 *   0x00 = 0.5 Hz
 *   0x01 = 1 Hz
 *   0x02 = 2 Hz
 *   0x03 = 4 Hz  ← we use this
 *   0x04 = 8 Hz  (Note: the register value written is 0x04, which maps to 4Hz based on sensor datasheet encoding)
 *   0x05 = 16 Hz
 *   0x06 = 32 Hz
 *   0x07 = 64 Hz (high noise at this rate)
 *
 * Returns 0 on success, negative on error.
 */
int MLX90640_SetRefreshRate(uint8_t slaveAddr, uint8_t refreshRate)
{
    uint16_t controlRegister1;
    uint16_t value;
    int error;

    //value = (refreshRate & 0x07)<<7;
    value = ((uint16_t)refreshRate << MLX90640_CTRL_REFRESH_SHIFT);
    value &= ~MLX90640_CTRL_REFRESH_MASK;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);
    if(error == MLX90640_NO_ERROR)
    {
        value = (controlRegister1 & MLX90640_CTRL_REFRESH_MASK) | value;
        error = MLX90640_I2CWrite(slaveAddr, MLX90640_CTRL_REG, value);
    }

    return error;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetRefreshRate(uint8_t slaveAddr)
 * Reads the currently configured refresh rate from the sensor's control register.
 * Returns the rate code (0–7, see SetRefreshRate for mapping),
 * or a negative value on I2C error.
 */
int MLX90640_GetRefreshRate(uint8_t slaveAddr)
{
    uint16_t controlRegister1;
    int refreshRate;
    int error;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);
    if(error != MLX90640_NO_ERROR)
    {
        return error;
    }
    refreshRate = (controlRegister1 & ~MLX90640_CTRL_REFRESH_MASK) >> MLX90640_CTRL_REFRESH_SHIFT;

    return refreshRate;
}

uint8_t MLX90640_ScanDevices(I2C_HandleTypeDef i2cport) {
	HAL_StatusTypeDef result;
	for (int i = 1; i < 128; i++) {
		result = HAL_I2C_IsDeviceReady(&i2cport, (uint16_t)(i << 1), 3, 5);

		if (result == HAL_ERROR) {
			uint32_t error_code = HAL_I2C_GetError(&i2cport);
//			printf("Addr 0x%02X Error! HAL_I2C_GetError: %lu\n", i, error_code);
			printf(".");
		}
		else if (result == HAL_OK) {
			printf("Found OK : 0x%02X\n", i);
			return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_SetInterleavedMode(uint8_t slaveAddr)
 * Switches the sensor to interleaved capture mode.
 * In this mode, subpage 0 captures all even rows and subpage 1 captures all
 * odd rows. This is the alternative to chess mode.
 *
 * Chess mode (used in our setup) is generally preferred as it provides
 * better spatial uniformity. Only switch to this for specific use cases.
 * Returns 0 on success, negative on error.
 */
int MLX90640_SetInterleavedMode(uint8_t slaveAddr)
{
    uint16_t controlRegister1;
    uint16_t value;
    int error;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);

    if(error == 0)
    {
        value = (controlRegister1 & ~MLX90640_CTRL_MEAS_MODE_MASK);
        error = MLX90640_I2CWrite(slaveAddr, MLX90640_CTRL_REG, value);
    }

    return error;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_SetChessMode(uint8_t slaveAddr)
 * Switches the sensor to chess (checkerboard) capture mode — this is what
 * we use. Subpage 0 and subpage 1 each capture alternating pixels in a
 * checkerboard pattern across the full 32x24 grid.
 *
 * Preferred over interleaved mode because it distributes both subpages
 * evenly across the image, reducing row-banding artifacts.
 * Returns 0 on success, negative on error.
 */
int MLX90640_SetChessMode(uint8_t slaveAddr)
{
    uint16_t controlRegister1;
    uint16_t value;
    int error;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);

    if(error == 0)
    {
        value = (controlRegister1 | MLX90640_CTRL_MEAS_MODE_MASK);
        error = MLX90640_I2CWrite(slaveAddr, MLX90640_CTRL_REG, value);
    }

    return error;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetCurMode(uint8_t slaveAddr)
 * Returns the current capture mode set on the sensor.
 *   0 = interleaved mode
 *   1 = chess mode
 * Returns a negative value on I2C error.
 */
int MLX90640_GetCurMode(uint8_t slaveAddr)
{
    uint16_t controlRegister1;
    int modeRAM;
    int error;

    error = MLX90640_I2CRead(slaveAddr, MLX90640_CTRL_REG, 1, &controlRegister1);
    if(error != 0)
    {
        return error;
    }
    modeRAM = (controlRegister1 & MLX90640_CTRL_MEAS_MODE_MASK) >> MLX90640_CTRL_MEAS_MODE_SHIFT;

    return modeRAM;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_CalculateTo(uint16_t *frameData, const paramsMLX90640 *params,
 *                      float emissivity, float tr, float *result)
 * The main temperature calculation function. Converts raw ADC pixel data from
 * one subpage into calibrated temperatures in degrees Celsius.
 *
 * Only updates the pixels belonging to the current subpage (frameData[833]).
 * Call this after EACH GetFrameData() call. After two calls (subpage 0 + 1),
 * result[] will have all 768 pixels populated.
 *
 * Parameters:
 *   frameData  - raw data from GetFrameData() (834 words)
 *   params     - calibration struct from ExtractParameters()
 *   emissivity - surface emissivity of the target (0.0–1.0). Use 0.95 for
 *                rubber/tire surfaces. Lower values = more reflective surfaces.
 *   tr         - reflected (ambient) temperature in °C. Typically ta - 8,
 *                where ta comes from MLX90640_GetTa(). Accounts for background
 *                IR radiation reflecting off the target into the sensor.
 *   result     - output array of 768 floats, indexed as [row * 32 + col]
 */
void MLX90640_CalculateTo(uint16_t *frameData, const paramsMLX90640 *params, float emissivity, float tr, float *result)
{
    float vdd;
    float ta;
    float ta4;
    float tr4;
    float taTr;
    float gain;
    float irDataCP[2];
    float irData;
    float alphaCompensated;
    uint8_t mode;
    int8_t ilPattern;
    int8_t chessPattern;
    int8_t pattern;
    int8_t conversionPattern;
    float Sx;
    float To;
    float alphaCorrR[4];
    int8_t range;
    uint16_t subPage;
    float ktaScale;
    float kvScale;
    float alphaScale;
    float kta;
    float kv;

    subPage = frameData[833];
    vdd = MLX90640_GetVdd(frameData, params);
    ta = MLX90640_GetTa(frameData, params);

    ta4 = (ta + 273.15f);
    ta4 = ta4 * ta4;
    ta4 = ta4 * ta4;
    tr4 = (tr + 273.15f);
    tr4 = tr4 * tr4;
    tr4 = tr4 * tr4;
    taTr = tr4 - (tr4-ta4)/emissivity;

    ktaScale = POW2(params->ktaScale);
    kvScale = POW2(params->kvScale);
    alphaScale = POW2(params->alphaScale);

    alphaCorrR[0] = 1 / (1 + params->ksTo[0] * 40);
    alphaCorrR[1] = 1 ;
    alphaCorrR[2] = (1 + params->ksTo[1] * params->ct[2]);
    alphaCorrR[3] = alphaCorrR[2] * (1 + params->ksTo[2] * (params->ct[3] - params->ct[2]));

//------------------------- Gain calculation -----------------------------------

    gain = (float)params->gainEE / (int16_t)frameData[778];

//------------------------- To calculation -------------------------------------
    mode = (frameData[832] & MLX90640_CTRL_MEAS_MODE_MASK) >> 5;

    irDataCP[0] = (int16_t)frameData[776] * gain;
    irDataCP[1] = (int16_t)frameData[808] * gain;

    irDataCP[0] = irDataCP[0] - params->cpOffset[0] * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    if( mode ==  params->calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - params->cpOffset[1] * (1 + params->cpKta * (ta - 25)) * (1 + params->cpKv * (vdd - 3.3f));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (params->cpOffset[1] + params->ilChessC[0]) * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    }

    for( int pixelNumber = 0; pixelNumber < 768; pixelNumber++)
    {
        ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
        chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2);
        conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);

        if(mode == 0)
        {
          pattern = ilPattern;
        }
        else
        {
          pattern = chessPattern;
        }

        if(pattern == frameData[833])
        {
            irData = (int16_t)frameData[pixelNumber] * gain;

            kta = params->kta[pixelNumber]/ktaScale;
            kv = params->kv[pixelNumber]/kvScale;
            irData = irData - params->offset[pixelNumber]*(1 + kta*(ta - 25.0f))*(1 + kv*(vdd - 3.3f));

            if(mode !=  params->calibrationModeEE)
            {
              irData = irData + params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
            }

            irData = irData - params->tgc * irDataCP[subPage];
            irData = irData / emissivity;

            alphaCompensated = SCALEALPHA*alphaScale/params->alpha[pixelNumber];
            alphaCompensated = alphaCompensated*(1 + params->KsTa * (ta - 25.0f));

            Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
            Sx = sqrtf(sqrtf(Sx)) * params->ksTo[1];

            To = sqrtf(sqrtf(irData/(alphaCompensated * (1 - params->ksTo[1] * 273.15f) + Sx) + taTr)) - 273.15f;

            if(To < params->ct[1])
            {
                range = 0;
            }
            else if(To < params->ct[2])
            {
                range = 1;
            }
            else if(To < params->ct[3])
            {
                range = 2;
            }
            else
            {
                range = 3;
            }

            To = sqrtf(sqrtf(irData / (alphaCompensated * alphaCorrR[range] * (1 + params->ksTo[range] * (To - params->ct[range]))) + taTr)) - 273.15f;

            result[pixelNumber] = To;
        }
    }
}





void MLX90640_CalculateTo_Hotbar(uint16_t *frameData, const paramsMLX90640 *params, float emissivity, float tr, float *result)
{
    float vdd;
    float ta;
    float ta4;
    float tr4;
    float taTr;
    float gain;
    float irDataCP[2];
    float irData;
    float alphaCompensated;
    uint8_t mode;
    int8_t ilPattern;
    int8_t chessPattern;
    int8_t pattern;
    int8_t conversionPattern;
    float Sx;
    float To;
    float alphaCorrR[4];
    int8_t range;
    uint16_t subPage;
    float ktaScale;
    float kvScale;
    float alphaScale;
    float kta;
    float kv;

    subPage = frameData[833];
    vdd = MLX90640_GetVdd(frameData, params);
    ta = MLX90640_GetTa(frameData, params);

    ta4 = (ta + 273.15f);
    ta4 = ta4 * ta4;
    ta4 = ta4 * ta4;
    tr4 = (tr + 273.15f);
    tr4 = tr4 * tr4;
    tr4 = tr4 * tr4;
    taTr = tr4 - (tr4-ta4)/emissivity;

    ktaScale = POW2(params->ktaScale);
    kvScale = POW2(params->kvScale);
    alphaScale = POW2(params->alphaScale);

    alphaCorrR[0] = 1 / (1 + params->ksTo[0] * 40);
    alphaCorrR[1] = 1 ;
    alphaCorrR[2] = (1 + params->ksTo[1] * params->ct[2]);
    alphaCorrR[3] = alphaCorrR[2] * (1 + params->ksTo[2] * (params->ct[3] - params->ct[2]));

//------------------------- Gain calculation -----------------------------------

    gain = (float)params->gainEE / (int16_t)frameData[778];

//------------------------- To calculation -------------------------------------
    mode = (frameData[832] & MLX90640_CTRL_MEAS_MODE_MASK) >> 5;

    irDataCP[0] = (int16_t)frameData[776] * gain;
    irDataCP[1] = (int16_t)frameData[808] * gain;

    irDataCP[0] = irDataCP[0] - params->cpOffset[0] * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    if( mode ==  params->calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - params->cpOffset[1] * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (params->cpOffset[1] + params->ilChessC[0]) * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    }

    for( int pixelNumber = 0; pixelNumber < 768; pixelNumber++)
    {
        ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
        chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2);
        conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);

        if(mode == 0)
        {
          pattern = ilPattern;
        }
        else
        {
          pattern = chessPattern;
        }

        if(pattern == frameData[833])
        {
            irData = (int16_t)frameData[pixelNumber] * gain;

            kta = params->kta[pixelNumber]/ktaScale;
            kv = params->kv[pixelNumber]/kvScale;
            irData = irData - params->offset[pixelNumber]*(1 + kta*(ta - 25.0f))*(1 + kv*(vdd - 3.3f));

            if(mode !=  params->calibrationModeEE)
            {
              irData = irData + params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
            }

            irData = irData - params->tgc * irDataCP[subPage];
            irData = irData / emissivity;

            alphaCompensated = SCALEALPHA*alphaScale/params->alpha[pixelNumber];
            alphaCompensated = alphaCompensated*(1 + params->KsTa * (ta - 25.0f));

            Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
            Sx = sqrtf(sqrtf(Sx)) * params->ksTo[1];

            To = sqrtf(sqrtf(irData/(alphaCompensated * (1 - params->ksTo[1] * 273.15f) + Sx) + taTr)) - 273.15f;

            if(To < params->ct[1])
            {
                range = 0;
            }
            else if(To < params->ct[2])
            {
                range = 1;
            }
            else if(To < params->ct[3])
            {
                range = 2;
            }
            else
            {
                range = 3;
            }

            To = sqrtf(sqrtf(irData / (alphaCompensated * alphaCorrR[range] * (1 + params->ksTo[range] * (To - params->ct[range]))) + taTr)) - 273.15f;

            result[pixelNumber] = To;
        }
    }
}


//------------------------------------------------------------------------------

/*
 * MLX90640_GetImage(uint16_t *frameData, const paramsMLX90640 *params, float *result)
 * A simplified alternative to CalculateTo() that outputs relative IR intensity
 * values instead of absolute temperatures. Does NOT account for emissivity or
 * reflected temperature — the output is unitless and uncalibrated.
 *
 * Useful for visualizing the thermal scene (e.g. finding hot spots) when you
 * don't need accurate temperature numbers. Not used in our current setup.
 * For real temperature measurements, always use MLX90640_CalculateTo() instead.
 */
void MLX90640_GetImage(uint16_t *frameData, const paramsMLX90640 *params, float *result)
{
    float vdd;
    float ta;
    float gain;
    float irDataCP[2];
    float irData;
    float alphaCompensated;
    uint8_t mode;
    int8_t ilPattern;
    int8_t chessPattern;
    int8_t pattern;
    int8_t conversionPattern;
    float image;
    uint16_t subPage;
    float ktaScale;
    float kvScale;
    float kta;
    float kv;

    subPage = frameData[833];
    vdd = MLX90640_GetVdd(frameData, params);
    ta = MLX90640_GetTa(frameData, params);

    ktaScale = POW2(params->ktaScale);
    kvScale = POW2(params->kvScale);

//------------------------- Gain calculation -----------------------------------

    gain = (float)params->gainEE / (int16_t)frameData[778];

//------------------------- Image calculation -------------------------------------

    mode = (frameData[832] & MLX90640_CTRL_MEAS_MODE_MASK) >> 5;

    irDataCP[0] = (int16_t)frameData[776] * gain;
    irDataCP[1] = (int16_t)frameData[808] * gain;

    irDataCP[0] = irDataCP[0] - params->cpOffset[0] * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    if( mode ==  params->calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - params->cpOffset[1] * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (params->cpOffset[1] + params->ilChessC[0]) * (1 + params->cpKta * (ta - 25.0f)) * (1 + params->cpKv * (vdd - 3.3f));
    }

    for( int pixelNumber = 352; pixelNumber < 448; pixelNumber++)
    {
        ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
        chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2);
        conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);

        if(mode == 0)
        {
          pattern = ilPattern;
        }
        else
        {
          pattern = chessPattern;
        }

        if(pattern == frameData[833])
        {
            irData = (int16_t)frameData[pixelNumber] * gain;

            kta = params->kta[pixelNumber]/ktaScale;
            kv = params->kv[pixelNumber]/kvScale;
            irData = irData - params->offset[pixelNumber]*(1 + kta*(ta - 25.0f))*(1 + kv*(vdd - 3.3f));

            if(mode !=  params->calibrationModeEE)
            {
              irData = irData + params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
            }

            irData = irData - params->tgc * irDataCP[subPage];

            alphaCompensated = params->alpha[pixelNumber];

            image = irData*alphaCompensated;

            result[pixelNumber] = image;
        }
    }
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetVdd(uint16_t *frameData, const paramsMLX90640 *params)
 * Calculates the sensor's measured supply voltage (VDD) in volts from the
 * auxiliary data embedded in the frame. Expected value is ~3.3fV.
 *
 * Used internally by GetTa() and CalculateTo() to compensate for any voltage
 * drift that would affect the pixel readings. You don't normally need to call
 * this directly.
 */
float MLX90640_GetVdd(uint16_t *frameData, const paramsMLX90640 *params)
{
    float vdd;
    float resolutionCorrection;

    uint16_t resolutionRAM;

    resolutionRAM = (frameData[832] & ~MLX90640_CTRL_RESOLUTION_MASK) >> MLX90640_CTRL_RESOLUTION_SHIFT;
    resolutionCorrection = POW2(params->resolutionEE) / POW2(resolutionRAM);
    vdd = (resolutionCorrection * (int16_t)frameData[810] - params->vdd25) / params->kVdd + 3.3f;

    return vdd;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetTa(uint16_t *frameData, const paramsMLX90640 *params)
 * Returns the ambient temperature (Ta) in °C — this is the temperature of the
 * sensor's die itself, not the scene being measured.
 *
 * Used in main.c to compute the reflected temperature (tr = ta - TA_SHIFT)
 * which is passed into CalculateTo(). Also stored in mlx_ambient so you can
 * monitor it in the debugger live expressions view.
 *
 * Call this after each GetFrameData() before calling CalculateTo().
 */
float MLX90640_GetTa(uint16_t *frameData, const paramsMLX90640 *params)
{
    int16_t ptat;
    float ptatArt;
    float vdd;
    float ta;

    vdd = MLX90640_GetVdd(frameData, params);

    ptat = (int16_t)frameData[800];

    ptatArt = (ptat / (ptat * params->alphaPTAT + (int16_t)frameData[768])) * POW2(18);

    ta = (ptatArt / (1 + params->KvPTAT * (vdd - 3.3f)) - params->vPTAT25);
    ta = ta / params->KtPTAT + 25;

    return ta;
}

//------------------------------------------------------------------------------

/*
 * MLX90640_GetSubPageNumber(uint16_t *frameData)
 * Returns which subpage (0 or 1) the provided frameData belongs to.
 * This is stored at frameData[833] and set automatically by GetFrameData().
 * Useful if you need to track subpage ordering explicitly.
 */
int MLX90640_GetSubPageNumber(uint16_t *frameData)
{
    return frameData[833];

}

//------------------------------------------------------------------------------
/*
 * MLX90640_BadPixelsCorrection(uint16_t *pixels, float *to, int mode, paramsMLX90640 *params)
 * Fixes broken or outlier pixels in the temperature output array by
 * interpolating from their neighbours.
 *
 * The list of bad pixels was identified at the factory and stored in EEPROM
 * (extracted into params->brokenPixels[] and params->outlierPixels[]).
 *
 * mode:
 *   1 = use nearest neighbour (faster, good enough for most cases)
 *   2 = use linear interpolation from all surrounding valid pixels
 *
 * Call this optionally after CalculateTo() if image quality matters.
 * Not currently called in our main loop.
 */
void MLX90640_BadPixelsCorrection(uint16_t *pixels, float *to, int mode, paramsMLX90640 *params)
{
    float ap[4];
    uint8_t pix;
    uint8_t line;
    uint8_t column;

    pix = 0;
    while(pixels[pix] != 0xFFFF)
    {
        line = pixels[pix]>>5;
        column = pixels[pix] - (line<<5);

        if(mode == 1)
        {
            if(line == 0)
            {
                if(column == 0)
                {
                    to[pixels[pix]] = to[33];
                }
                else if(column == 31)
                {
                    to[pixels[pix]] = to[62];
                }
                else
                {
                    to[pixels[pix]] = (to[pixels[pix]+31] + to[pixels[pix]+33])/2.0;
                }
            }
            else if(line == 23)
            {
                if(column == 0)
                {
                    to[pixels[pix]] = to[705];
                }
                else if(column == 31)
                {
                    to[pixels[pix]] = to[734];
                }
                else
                {
                    to[pixels[pix]] = (to[pixels[pix]-33] + to[pixels[pix]-31])/2.0;
                }
            }
            else if(column == 0)
            {
                to[pixels[pix]] = (to[pixels[pix]-31] + to[pixels[pix]+33])/2.0;
            }
            else if(column == 31)
            {
                to[pixels[pix]] = (to[pixels[pix]-33] + to[pixels[pix]+31])/2.0;
            }
            else
            {
                ap[0] = to[pixels[pix]-33];
                ap[1] = to[pixels[pix]-31];
                ap[2] = to[pixels[pix]+31];
                ap[3] = to[pixels[pix]+33];
                to[pixels[pix]] = GetMedian(ap,4);
            }
        }
        else
        {
            if(column == 0)
            {
                to[pixels[pix]] = to[pixels[pix]+1];
            }
            else if(column == 1 || column == 30)
            {
                to[pixels[pix]] = (to[pixels[pix]-1]+to[pixels[pix]+1])/2.0;
            }
            else if(column == 31)
            {
                to[pixels[pix]] = to[pixels[pix]-1];
            }
            else
            {
                if(IsPixelBad(pixels[pix]-2,params) == 0 && IsPixelBad(pixels[pix]+2,params) == 0)
                {
                    ap[0] = to[pixels[pix]+1] - to[pixels[pix]+2];
                    ap[1] = to[pixels[pix]-1] - to[pixels[pix]-2];
                    if(fabs(ap[0]) > fabs(ap[1]))
                    {
                        to[pixels[pix]] = to[pixels[pix]-1] + ap[1];
                    }
                    else
                    {
                        to[pixels[pix]] = to[pixels[pix]+1] + ap[0];
                    }
                }
                else
                {
                    to[pixels[pix]] = (to[pixels[pix]-1]+to[pixels[pix]+1])/2.0;
                }
            }
        }
        pix = pix + 1;
    }
}

//------------------------------------------------------------------------------

static void ExtractVDDParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int8_t kVdd;
    int16_t vdd25;

    kVdd = MLX90640_MS_BYTE(eeData[51]);

    vdd25 = MLX90640_LS_BYTE(eeData[51]);
    vdd25 = ((vdd25 - 256) << 5) - 8192;

    mlx90640->kVdd = 32 * kVdd;
    mlx90640->vdd25 = vdd25;
}

//------------------------------------------------------------------------------

static void ExtractPTATParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    float KvPTAT;
    float KtPTAT;
    int16_t vPTAT25;
    float alphaPTAT;

    KvPTAT = (eeData[50] & MLX90640_MSBITS_6_MASK) >> 10;
    if(KvPTAT > 31)
    {
        KvPTAT = KvPTAT - 64;
    }
    KvPTAT = KvPTAT/4096;

    KtPTAT = eeData[50] & MLX90640_LSBITS_10_MASK;
    if(KtPTAT > 511)
    {
        KtPTAT = KtPTAT - 1024;
    }
    KtPTAT = KtPTAT/8;

    vPTAT25 = eeData[49];

    alphaPTAT = (eeData[16] & MLX90640_NIBBLE4_MASK) / POW2(14) + 8.0f;

    mlx90640->KvPTAT = KvPTAT;
    mlx90640->KtPTAT = KtPTAT;
    mlx90640->vPTAT25 = vPTAT25;
    mlx90640->alphaPTAT = alphaPTAT;
}

//------------------------------------------------------------------------------

static void ExtractGainParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    mlx90640->gainEE = (int16_t)eeData[48];;
}

//------------------------------------------------------------------------------

static void ExtractTgcParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    mlx90640->tgc = (int8_t)MLX90640_LS_BYTE(eeData[60]) / 32.0f;
}

//------------------------------------------------------------------------------

static void ExtractResolutionParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    uint8_t resolutionEE;
    resolutionEE = (eeData[56] & 0x3000) >> 12;

    mlx90640->resolutionEE = resolutionEE;
}

//------------------------------------------------------------------------------

static void ExtractKsTaParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    mlx90640->KsTa = (int8_t)MLX90640_MS_BYTE(eeData[60]) / 8192.0f;
}

//------------------------------------------------------------------------------

static void ExtractKsToParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int32_t KsToScale;
    int8_t step;

    step = ((eeData[63] & 0x3000) >> 12) * 10;

    mlx90640->ct[0] = -40;
    mlx90640->ct[1] = 0;
    mlx90640->ct[2] = MLX90640_NIBBLE2(eeData[63]);
    mlx90640->ct[3] = MLX90640_NIBBLE3(eeData[63]);

    mlx90640->ct[2] = mlx90640->ct[2]*step;
    mlx90640->ct[3] = mlx90640->ct[2] + mlx90640->ct[3]*step;
    mlx90640->ct[4] = 400;

    KsToScale = MLX90640_NIBBLE1(eeData[63]) + 8;
    KsToScale = 1UL << KsToScale;

    mlx90640->ksTo[0] = (int8_t)MLX90640_LS_BYTE(eeData[61]) / (float)KsToScale;
    mlx90640->ksTo[1] = (int8_t)MLX90640_MS_BYTE(eeData[61]) / (float)KsToScale;
    mlx90640->ksTo[2] = (int8_t)MLX90640_LS_BYTE(eeData[62]) / (float)KsToScale;
    mlx90640->ksTo[3] = (int8_t)MLX90640_MS_BYTE(eeData[62]) / (float)KsToScale;
    mlx90640->ksTo[4] = -0.0002;
}

//------------------------------------------------------------------------------

static void ExtractAlphaParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int accRow[24];
    int accColumn[32];
    int p = 0;
    int alphaRef;
    uint8_t alphaScale;
    uint8_t accRowScale;
    uint8_t accColumnScale;
    uint8_t accRemScale;
    float alphaTemp[768];
    float temp;


    accRemScale = MLX90640_NIBBLE1(eeData[32]);
    accColumnScale = MLX90640_NIBBLE2(eeData[32]);
    accRowScale = MLX90640_NIBBLE3(eeData[32]);
    alphaScale = MLX90640_NIBBLE4(eeData[32]) + 30;
    alphaRef = eeData[33];

    for(int i = 0; i < 6; i++)
    {
        p = i * 4;
        accRow[p + 0] = MLX90640_NIBBLE1(eeData[34 + i]);
        accRow[p + 1] = MLX90640_NIBBLE2(eeData[34 + i]);
        accRow[p + 2] = MLX90640_NIBBLE3(eeData[34 + i]);
        accRow[p + 3] = MLX90640_NIBBLE4(eeData[34 + i]);
    }

    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        if (accRow[i] > 7)
        {
            accRow[i] = accRow[i] - 16;
        }
    }

    for(int i = 0; i < 8; i++)
    {
        p = i * 4;
        accColumn[p + 0] = MLX90640_NIBBLE1(eeData[40 + i]);
        accColumn[p + 1] = MLX90640_NIBBLE2(eeData[40 + i]);
        accColumn[p + 2] = MLX90640_NIBBLE3(eeData[40 + i]);
        accColumn[p + 3] = MLX90640_NIBBLE4(eeData[40 + i]);
    }

    for(int i = 0; i < MLX90640_COLUMN_NUM; i++)
    {
        if (accColumn[i] > 7)
        {
            accColumn[i] = accColumn[i] - 16;
        }
    }

    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        for(int j = 0; j < MLX90640_COLUMN_NUM; j ++)
        {
            p = 32 * i +j;
            alphaTemp[p] = (eeData[64 + p] & 0x03F0) >> 4;
            if (alphaTemp[p] > 31)
            {
                alphaTemp[p] = alphaTemp[p] - 64;
            }
            alphaTemp[p] = alphaTemp[p]*(1 << accRemScale);
            alphaTemp[p] = (alphaRef + (accRow[i] << accRowScale) + (accColumn[j] << accColumnScale) + alphaTemp[p]);
            alphaTemp[p] = alphaTemp[p] / POW2(alphaScale);
            alphaTemp[p] = alphaTemp[p] - mlx90640->tgc * (mlx90640->cpAlpha[0] + mlx90640->cpAlpha[1])/2;
            alphaTemp[p] = SCALEALPHA/alphaTemp[p];
        }
    }

    temp = alphaTemp[0];
    for(int i = 1; i < MLX90640_PIXEL_NUM; i++)
    {
        if (alphaTemp[i] > temp)
        {
            temp = alphaTemp[i];
        }
    }

    alphaScale = 0;
    while(temp < 32767.4)
    {
        temp = temp*2;
        alphaScale = alphaScale + 1;
    }

    for(int i = 0; i < MLX90640_PIXEL_NUM; i++)
    {
        temp = alphaTemp[i] * POW2(alphaScale);
        mlx90640->alpha[i] = (temp + 0.5);

    }

    mlx90640->alphaScale = alphaScale;

}

//------------------------------------------------------------------------------

static void ExtractOffsetParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int occRow[24];
    int occColumn[32];
    int p = 0;
    int16_t offsetRef;
    uint8_t occRowScale;
    uint8_t occColumnScale;
    uint8_t occRemScale;


    occRemScale = MLX90640_NIBBLE1(eeData[16]);
    occColumnScale = MLX90640_NIBBLE2(eeData[16]);
    occRowScale = MLX90640_NIBBLE3(eeData[16]);
    offsetRef = (int16_t)eeData[17];

    for(int i = 0; i < 6; i++)
    {
        p = i * 4;
        occRow[p + 0] = MLX90640_NIBBLE1(eeData[18 + i]);
        occRow[p + 1] = MLX90640_NIBBLE2(eeData[18 + i]);
        occRow[p + 2] = MLX90640_NIBBLE3(eeData[18 + i]);
        occRow[p + 3] = MLX90640_NIBBLE4(eeData[18 + i]);
    }

    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        if (occRow[i] > 7)
        {
            occRow[i] = occRow[i] - 16;
        }
    }

    for(int i = 0; i < 8; i++)
    {
        p = i * 4;
        occColumn[p + 0] = MLX90640_NIBBLE1(eeData[24 + i]);
        occColumn[p + 1] = MLX90640_NIBBLE2(eeData[24 + i]);
        occColumn[p + 2] = MLX90640_NIBBLE3(eeData[24 + i]);
        occColumn[p + 3] = MLX90640_NIBBLE4(eeData[24 + i]);
    }

    for(int i = 0; i < MLX90640_COLUMN_NUM; i ++)
    {
        if (occColumn[i] > 7)
        {
            occColumn[i] = occColumn[i] - 16;
        }
    }

    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        for(int j = 0; j < MLX90640_COLUMN_NUM; j ++)
        {
            p = 32 * i +j;
            mlx90640->offset[p] = (eeData[64 + p] & MLX90640_MSBITS_6_MASK) >> 10;
            if (mlx90640->offset[p] > 31)
            {
                mlx90640->offset[p] = mlx90640->offset[p] - 64;
            }
            mlx90640->offset[p] = mlx90640->offset[p]*(1 << occRemScale);
            mlx90640->offset[p] = (offsetRef + (occRow[i] << occRowScale) + (occColumn[j] << occColumnScale) + mlx90640->offset[p]);
        }
    }
}

//------------------------------------------------------------------------------

static void ExtractKtaPixelParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int p = 0;
    int8_t KtaRC[4];
    uint8_t ktaScale1;
    uint8_t ktaScale2;
    uint8_t split;
    float ktaTemp[768];
    float temp;

    KtaRC[0] = (int8_t)MLX90640_MS_BYTE(eeData[54]);;
    KtaRC[2] = (int8_t)MLX90640_LS_BYTE(eeData[54]);;
    KtaRC[1] = (int8_t)MLX90640_MS_BYTE(eeData[55]);;
    KtaRC[3] = (int8_t)MLX90640_LS_BYTE(eeData[55]);;

    ktaScale1 = MLX90640_NIBBLE2(eeData[56]) + 8;
    ktaScale2 = MLX90640_NIBBLE1(eeData[56]);

    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        for(int j = 0; j < MLX90640_COLUMN_NUM; j ++)
        {
            p = 32 * i +j;
            split = 2*(p/32 - (p/64)*2) + p%2;
            ktaTemp[p] = (eeData[64 + p] & 0x000E) >> 1;
            if (ktaTemp[p] > 3)
            {
                ktaTemp[p] = ktaTemp[p] - 8;
            }
            ktaTemp[p] = ktaTemp[p] * (1 << ktaScale2);
            ktaTemp[p] = KtaRC[split] + ktaTemp[p];
            ktaTemp[p] = ktaTemp[p] / POW2(ktaScale1);

        }
    }

    temp = fabs(ktaTemp[0]);
    for(int i = 1; i < MLX90640_PIXEL_NUM; i++)
    {
        if (fabs(ktaTemp[i]) > temp)
        {
            temp = fabs(ktaTemp[i]);
        }
    }

    ktaScale1 = 0;
    while(temp < 63.4)
    {
        temp = temp*2;
        ktaScale1 = ktaScale1 + 1;
    }

    for(int i = 0; i < MLX90640_PIXEL_NUM; i++)
    {
        temp = ktaTemp[i] * POW2(ktaScale1);
        if (temp < 0)
        {
            mlx90640->kta[i] = (temp - 0.5);
        }
        else
        {
            mlx90640->kta[i] = (temp + 0.5);
        }

    }

    mlx90640->ktaScale = ktaScale1;
}


//------------------------------------------------------------------------------

static void ExtractKvPixelParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    int p = 0;
    int8_t KvT[4];
    int8_t KvRoCo;
    int8_t KvRoCe;
    int8_t KvReCo;
    int8_t KvReCe;
    uint8_t kvScale;
    uint8_t split;
    float kvTemp[768];
    float temp;

    KvRoCo = MLX90640_NIBBLE4(eeData[52]);
    if (KvRoCo > 7)
    {
        KvRoCo = KvRoCo - 16;
    }
    KvT[0] = KvRoCo;

    KvReCo = MLX90640_NIBBLE3(eeData[52]);
    if (KvReCo > 7)
    {
        KvReCo = KvReCo - 16;
    }
    KvT[2] = KvReCo;

    KvRoCe = MLX90640_NIBBLE2(eeData[52]);
    if (KvRoCe > 7)
    {
        KvRoCe = KvRoCe - 16;
    }
    KvT[1] = KvRoCe;

    KvReCe = MLX90640_NIBBLE1(eeData[52]);
    if (KvReCe > 7)
    {
        KvReCe = KvReCe - 16;
    }
    KvT[3] = KvReCe;

    kvScale = MLX90640_NIBBLE3(eeData[56]);


    for(int i = 0; i < MLX90640_LINE_NUM; i++)
    {
        for(int j = 0; j < MLX90640_COLUMN_NUM; j ++)
        {
            p = 32 * i +j;
            split = 2*(p/32 - (p/64)*2) + p%2;
            kvTemp[p] = KvT[split];
            kvTemp[p] = kvTemp[p] / POW2(kvScale);
        }
    }

    temp = fabs(kvTemp[0]);
    for(int i = 1; i < MLX90640_PIXEL_NUM; i++)
    {
        if (fabs(kvTemp[i]) > temp)
        {
            temp = fabs(kvTemp[i]);
        }
    }

    kvScale = 0;
    while(temp < 63.4)
    {
        temp = temp*2;
        kvScale = kvScale + 1;
    }

    for(int i = 0; i < MLX90640_PIXEL_NUM; i++)
    {
        temp = kvTemp[i] * POW2(kvScale);
        if (temp < 0)
        {
            mlx90640->kv[i] = (temp - 0.5);
        }
        else
        {
            mlx90640->kv[i] = (temp + 0.5);
        }

    }

    mlx90640->kvScale = kvScale;
}

//------------------------------------------------------------------------------

static void ExtractCPParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    float alphaSP[2];
    int16_t offsetSP[2];
    float cpKv;
    float cpKta;
    uint8_t alphaScale;
    uint8_t ktaScale1;
    uint8_t kvScale;

    alphaScale = MLX90640_NIBBLE4(eeData[32]) + 27;

    offsetSP[0] = (eeData[58] & MLX90640_LSBITS_10_MASK);
    if (offsetSP[0] > 511)
    {
        offsetSP[0] = offsetSP[0] - 1024;
    }

    offsetSP[1] = (eeData[58] & MLX90640_MSBITS_6_MASK) >> 10;
    if (offsetSP[1] > 31)
    {
        offsetSP[1] = offsetSP[1] - 64;
    }
    offsetSP[1] = offsetSP[1] + offsetSP[0];

    alphaSP[0] = (eeData[57] & MLX90640_LSBITS_10_MASK);
    if (alphaSP[0] > 511)
    {
        alphaSP[0] = alphaSP[0] - 1024;
    }
    alphaSP[0] = alphaSP[0] /  POW2(alphaScale);

    alphaSP[1] = (eeData[57] & MLX90640_MSBITS_6_MASK) >> 10;
    if (alphaSP[1] > 31)
    {
        alphaSP[1] = alphaSP[1] - 64;
    }
    alphaSP[1] = (1 + alphaSP[1]/128) * alphaSP[0];

    cpKta = (int8_t)MLX90640_LS_BYTE(eeData[59]);

    ktaScale1 = MLX90640_NIBBLE2(eeData[56]) + 8;
    mlx90640->cpKta = cpKta / POW2(ktaScale1);

    cpKv = (int8_t)MLX90640_MS_BYTE(eeData[59]);

    kvScale = MLX90640_NIBBLE3(eeData[56]);
    mlx90640->cpKv = cpKv / POW2(kvScale);

    mlx90640->cpAlpha[0] = alphaSP[0];
    mlx90640->cpAlpha[1] = alphaSP[1];
    mlx90640->cpOffset[0] = offsetSP[0];
    mlx90640->cpOffset[1] = offsetSP[1];
}

//------------------------------------------------------------------------------

static void ExtractCILCParameters(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    float ilChessC[3];
    uint8_t calibrationModeEE;

    calibrationModeEE = (eeData[10] & 0x0800) >> 4;
    calibrationModeEE = calibrationModeEE ^ 0x80;

    ilChessC[0] = (eeData[53] & 0x003F);
    if (ilChessC[0] > 31)
    {
        ilChessC[0] = ilChessC[0] - 64;
    }
    ilChessC[0] = ilChessC[0] / 16.0f;

    ilChessC[1] = (eeData[53] & 0x07C0) >> 6;
    if (ilChessC[1] > 15)
    {
        ilChessC[1] = ilChessC[1] - 32;
    }
    ilChessC[1] = ilChessC[1] / 2.0f;

    ilChessC[2] = (eeData[53] & 0xF800) >> 11;
    if (ilChessC[2] > 15)
    {
        ilChessC[2] = ilChessC[2] - 32;
    }
    ilChessC[2] = ilChessC[2] / 8.0f;

    mlx90640->calibrationModeEE = calibrationModeEE;
    mlx90640->ilChessC[0] = ilChessC[0];
    mlx90640->ilChessC[1] = ilChessC[1];
    mlx90640->ilChessC[2] = ilChessC[2];
}

//------------------------------------------------------------------------------

static int ExtractDeviatingPixels(uint16_t *eeData, paramsMLX90640 *mlx90640)
{
    uint16_t pixCnt = 0;
    uint16_t brokenPixCnt = 0;
    uint16_t outlierPixCnt = 0;
    int warn = 0;
    int i;

    for(pixCnt = 0; pixCnt<5; pixCnt++)
    {
        mlx90640->brokenPixels[pixCnt] = 0xFFFF;
        mlx90640->outlierPixels[pixCnt] = 0xFFFF;
    }

    pixCnt = 0;
    while (pixCnt < MLX90640_PIXEL_NUM && brokenPixCnt < 5 && outlierPixCnt < 5)
    {
        if(eeData[pixCnt+64] == 0)
        {
            mlx90640->brokenPixels[brokenPixCnt] = pixCnt;
            brokenPixCnt = brokenPixCnt + 1;
        }
        else if((eeData[pixCnt+64] & 0x0001) != 0)
        {
            mlx90640->outlierPixels[outlierPixCnt] = pixCnt;
            outlierPixCnt = outlierPixCnt + 1;
        }

        pixCnt = pixCnt + 1;

    }

    if(brokenPixCnt > 4)
    {
        warn = -MLX90640_BROKEN_PIXELS_NUM_ERROR;
    }
    else if(outlierPixCnt > 4)
    {
        warn = -MLX90640_OUTLIER_PIXELS_NUM_ERROR;
    }
    else if((brokenPixCnt + outlierPixCnt) > 4)
    {
        warn = -MLX90640_BAD_PIXELS_NUM_ERROR;
    }
    else
    {
        for(pixCnt=0; pixCnt<brokenPixCnt; pixCnt++)
        {
            for(i=pixCnt+1; i<brokenPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640->brokenPixels[pixCnt],mlx90640->brokenPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

        for(pixCnt=0; pixCnt<outlierPixCnt; pixCnt++)
        {
            for(i=pixCnt+1; i<outlierPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640->outlierPixels[pixCnt],mlx90640->outlierPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

        for(pixCnt=0; pixCnt<brokenPixCnt; pixCnt++)
        {
            for(i=0; i<outlierPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640->brokenPixels[pixCnt],mlx90640->outlierPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

    }


    return warn;

}

//------------------------------------------------------------------------------

 static int CheckAdjacentPixels(uint16_t pix1, uint16_t pix2)
 {

     int pixPosDif;
     uint16_t lp1 = pix1 >> 5;
     uint16_t lp2 = pix2 >> 5;
     uint16_t cp1 = pix1 - (lp1 << 5);
     uint16_t cp2 = pix2 - (lp2 << 5);

     pixPosDif = lp1 - lp2;
     if(pixPosDif > -2 && pixPosDif < 2)
     {
        pixPosDif = cp1 - cp2;
        if(pixPosDif > -2 && pixPosDif < 2)
        {
            return -6;
        }

     }

     return 0;
 }

//------------------------------------------------------------------------------

static float GetMedian(float *values, int n)
 {
    float temp;

    for(int i=0; i<n-1; i++)
    {
        for(int j=i+1; j<n; j++)
        {
            if(values[j] < values[i])
            {
                temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }

    if(n%2==0)
    {
        return ((values[n/2] + values[n/2 - 1]) / 2.0);

    }
    else
    {
        return values[n/2];
    }

 }

//------------------------------------------------------------------------------

static int IsPixelBad(uint16_t pixel,paramsMLX90640 *params)
{
    for(int i=0; i<5; i++)
    {
        if(pixel == params->outlierPixels[i] || pixel == params->brokenPixels[i])
        {
            return 1;
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
