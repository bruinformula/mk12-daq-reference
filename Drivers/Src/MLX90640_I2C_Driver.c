/*
 * MLX90640_I2C_Driver.c
 *
 * Low-level I2C communication layer for the MLX90640 thermal sensor.
 * These functions handle the raw byte transfers over I2C between the STM32
 * and the sensor. You should not need to call most of these directly —
 * the higher-level MLX90640_API functions call them internally.
 *
 * The one exception is MLX90640_I2CFreqSet(), which you may need if you
 * want to change the I2C clock speed at runtime.
 */

#include "MLX90640_I2C_Driver.h"
//extern I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef *i2cHandle;

/*
 * MLX90640_I2CInit()
 * Placeholder for I2C peripheral initialization.
 * Currently unused because MX_I2C2_Init() is already called in main.c
 * before the sensor is touched. Nothing to do here.
 */
void MLX90640_I2CInit(I2C_HandleTypeDef *i2c) {
	i2cHandle = i2c;
}

/*
 * MLX90640_I2CFreqSet(int freq)
 * Changes the I2C clock speed at runtime.
 * 'freq' is a raw STM32 timing register value (not a kHz number directly).
 * To get the correct value for a given speed, use STM32CubeMX's I2C timing
 * calculator. The sensor supports up to 1 MHz (Fast Mode Plus).
 * This re-initializes the I2C peripheral, so don't call it mid-frame.
 */
void MLX90640_I2CFreqSet(int freq) {
	// Frequency is in kHz
	HAL_I2C_DeInit(i2cHandle);
	i2cHandle->Init.Timing = freq;
	HAL_I2C_Init(i2cHandle);

}

/*
 * MLX90640_I2CGeneralReset()
 * Sends an I2C General Call reset command (address 0x00, data 0x06).
 * This resets ALL devices on the I2C bus simultaneously, not just the MLX90640.
 * Used internally by MLX90640_TriggerMeasurement() to latch a new measurement.
 * Returns 0 on success, -1 if the I2C transmission failed.
 */
int MLX90640_I2CGeneralReset(void) {
	uint8_t reset_cmd = 0x06;
	int ack = HAL_I2C_Master_Transmit(i2cHandle, 0x00 << 1, &reset_cmd, 1, HAL_MAX_DELAY);
	if (ack != HAL_OK) {
		return -1;
	}
	return 0;
}

/*
 * MLX90640_getDeviceId(uint8_t slaveAddr, uint16_t* device_id)
 * Reads the device ID register from the sensor's EEPROM (address 0x2407).
 * The returned value should be 0x4640 for a genuine MLX90640.
 * Useful as a sanity check to confirm you're talking to the right chip.
 * Returns 0 on success, negative on I2C error.
 */
int MLX90640_getDeviceId(uint8_t slaveAddr, uint16_t* device_id) {
	// 4640 should be the Device ID Returned
	return MLX90640_I2CRead(slaveAddr, 0x2407, 1, device_id);
}

/*
 * I2CScan()
 * Scans all 128 possible I2C addresses and prints which ones respond.
 * Output goes to UART via printf (requires a working COM port / SWO).
 * The MLX90640 should show up at 0x33 (factory default).
 * Call this during bringup/debugging if the sensor isn't responding.
 */
void I2CScan() {
	// Simple helper function to verify that the slaveAddr was still 0x33, factory default
	printf("Scanning I2C Bus\r\n");
	for (uint8_t address = 0; address < 128; address++) {
		if (HAL_I2C_IsDeviceReady(i2cHandle, (address << 1), 1, 100) == HAL_OK) {
			printf("FOUND: 0x%02X\n", address);
		} else {
			printf("NOT FOUND: 0x%02X\n", address);
		}
	}
	printf("Scan Complete\r\n");
}

/*
 * MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
 *                  uint16_t nMemAddressRead, uint16_t *data)
 * Reads 'nMemAddressRead' 16-bit words from the sensor starting at 'startAddress'.
 * Results are stored in the 'data' array as correctly-ordered uint16_t values.
 *
 * Internally handles the byte-swap (endian correction): the sensor sends bytes
 * big-endian but HAL delivers them little-endian, so each pair is swapped here.
 *
 * Called by virtually every API function — you should not need to call this directly.
 * Returns 0 on success, -1 on I2C failure.
 */
int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
		uint16_t nMemAddressRead, uint16_t *data) {
	uint8_t* pData = (uint8_t*) data;
	int ack = HAL_I2C_Mem_Read(i2cHandle, (slaveAddr << 1), startAddress,
	I2C_MEMADD_SIZE_16BIT, pData, 2 * nMemAddressRead, 500);
	if (ack != HAL_OK) {
		return -1;
	}

	// Perform Endian Conversion on the received data
	for (int k = 0; k < nMemAddressRead * 2; k += 2) {
		uint8_t temp = pData[k+1];
		pData[k+1] = pData[k];
		pData[k] = temp;
	}

	return 0;

}

int MLX90640_I2CRead_DMA(uint8_t slaveAddr, uint16_t startAddress,
		uint16_t nMemAddressRead, uint16_t *data) {
	uint8_t* pData = (uint8_t*) data;
	int ack = HAL_I2C_Mem_Read_DMA(i2cHandle, (slaveAddr << 1), startAddress,
	I2C_MEMADD_SIZE_16BIT, pData, 2 * nMemAddressRead);
	if (ack != HAL_OK) {
		return -1;
	}

	return 0;

}

/*
 * MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
 * Writes a single 16-bit word to a sensor register at 'writeAddress'.
 * Sends the data big-endian (MSB first) as required by the sensor protocol.
 *
 * After writing, it reads back the register and verifies the value was stored
 * correctly. This is important for configuration registers like refresh rate
 * and measurement mode.
 *
 * Returns:
 *   0   = success
 *  -1   = I2C write failed
 *  -2   = write succeeded but readback didn't match (sensor didn't accept the value)
 */
int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
	uint8_t pData[2];
	pData[0] = (uint8_t)((data >> 8) & 0xFF); // MSB
	pData[1] = (uint8_t)(data & 0xFF); // LSB
	// Perform Endian Conversion on data to write

	int ack = HAL_I2C_Mem_Write(i2cHandle, (slaveAddr << 1), writeAddress,
	I2C_MEMADD_SIZE_16BIT, pData, sizeof(pData), 500);

	if (ack != HAL_OK) {
		return -1;
	}

	uint16_t dataCheck;
	MLX90640_I2CRead(slaveAddr, writeAddress, 1, &dataCheck);
	if (dataCheck != data) return -2;

	return 0;
}

