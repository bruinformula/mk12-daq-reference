#include "mcp3464.h"
#include "main.h"

/* --- debug readback buffers — file scope guarantees debugger visibility --- */
static uint8_t dbg_tx0[2], dbg_rx0[2];  /* CONFIG0 */
static uint8_t dbg_tx1[2], dbg_rx1[2];  /* CONFIG3 */
static uint8_t dbg_tx2[2], dbg_rx2[2];  /* IRQ     */
static uint8_t dbg_tx3[2], dbg_rx3[2];  /* MUX     */
static uint8_t dbg_adc_tx[3], dbg_adc_rx[3]; /* ADCDATA polling test */

extern volatile uint32_t sampleCount;   /* defined in main.c */
/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static inline void CS_Low(MCP3464_Handle *h)
{
    HAL_GPIO_WritePin(h->csPort, h->csPin, GPIO_PIN_RESET);
}

static inline void CS_High(MCP3464_Handle *h)
{
    HAL_GPIO_WritePin(h->csPort, h->csPin, GPIO_PIN_SET);
}

static inline uint8_t is_scan_mode(MCP3464_Handle *h)
{
    return (h->config.reg.config3.DATA_FORMAT == FORMAT_32_CHID);
}

/* --------------------------------------------------------------------------
 * Builder functions — convert bitfields to SPI bytes
 * -------------------------------------------------------------------------- */
static uint8_t build_config0(MCP3464_Handle *h)
{
    return (h->config.reg.config0.VREF_SEL   << 7) |
           (h->config.reg.config0.PARTIAL_SD  << 6) |
           (h->config.reg.config0.CLK_SEL     << 4) |
           (h->config.reg.config0.CS_SEL      << 2) |
           (h->config.reg.config0.ADC_MODE);
}

static uint8_t build_config1(MCP3464_Handle *h)
{
    return (h->config.reg.config1.PRE << 6) |
           (h->config.reg.config1.OSR << 2);
}

static uint8_t build_config2(MCP3464_Handle *h)
{
    return (h->config.reg.config2.BOOST    << 6) |
           (h->config.reg.config2.GAIN     << 3) |
           (h->config.reg.config2.AZ_MUX   << 2) |
           (h->config.reg.config2.AZ_REF   << 1) |
           (h->config.reg.config2.RESERVED);
}

static uint8_t build_config3(MCP3464_Handle *h)
{
    return (h->config.reg.config3.CONV_MODE   << 6) |
           (h->config.reg.config3.DATA_FORMAT << 4) |
           (h->config.reg.config3.CRC_FORMAT  << 3) |
           (h->config.reg.config3.EN_CRCCOM   << 2) |
           (h->config.reg.config3.EN_OFFCAL   << 1) |
           (h->config.reg.config3.EN_GAINCAL);
}

static uint8_t build_irq(MCP3464_Handle *h)
{
    return (h->config.reg.irq.IRQ_MODE1  << 3) |
           (h->config.reg.irq.IRQ_MODE0  << 2) |
           (h->config.reg.irq.EN_FASTCMD << 1) |
           (h->config.reg.irq.EN_STP);
}

static uint8_t build_mux(MCP3464_Handle *h)
{
    return (h->config.reg.mux.MUX_VIN_POS << 4) |
           (h->config.reg.mux.MUX_VIN_NEG);
}

/* --------------------------------------------------------------------------
 * MCP3464_Init
 *
 * Issues a full device reset (fast command 0x78), then performs a single
 * incremental write covering CONFIG0→MUX so every relevant register is
 * programmed in one CS assertion.
 * -------------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3464_Init(MCP3464_Handle *h)
{
	HAL_StatusTypeDef status;

    h->result          = 0;
    h->voltage         = 0.0f;
    h->dataReady       = 0;
    h->running         = 0;
    h->transferPending = 0;
	h->lastChannel     = 0xFFU;
	h->readyMask       = 0U;
	h->lastDrdyLevel   = GPIO_PIN_SET;
	h->lastRxStatus    = 0U;
	h->lastStartStatus = (uint8_t)HAL_OK;
	h->extiCount       = 0U;
	h->armAttemptCount = 0U;
	h->armAcceptedCount = 0U;
	h->startKickCount  = 0U;
	h->dmaStartOkCount = 0U;
	h->dmaStartFailCount = 0U;
	h->dmaCompleteCount = 0U;
	h->spiErrorCount   = 0U;
	h->droppedBusyCount = 0U;
	h->droppedStoppedCount = 0U;
	h->lastSpiErrorCode = 0U;

	for (uint32_t i = 0; i < MCP3464_SCAN_CHANNEL_COUNT; i++) {
		h->scanResult[i] = 0;
		h->scanVoltage[i] = 0.0f;
		h->channelSampleCount[i] = 0U;
		h->channelSampleRate[i] = 0.0f;
		h->sampleRateLastCount[i] = 0U;
	}

    h->sampleRateLastUpdateMs = HAL_GetTick();
	h->lastSpiErrorCode = 0U;

    CS_High(h);
    HAL_Delay(1);

    /* Full reset — entire register map returns to power-on defaults */
//    HAL_Delay(1000);
//    uint8_t rst = MCP3464_FCMD_RESET;
//    CS_Low(h);
//    HAL_SPI_Transmit(h->hspi, &rst, 1, 10);
//    CS_High(h);
//    HAL_SPIEx_FlushRxFifo(h->hspi);
//    HAL_Delay(10);
    /* Full reset — entire register map returns to power-on defaults */
    uint8_t rst_pattern[8] = {
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET,
        MCP3464_FCMD_RESET
    };

    CS_Low(h);
    HAL_Delay(100);   /* hold CS low before transmitting so scope can trigger */
	status = HAL_SPI_Transmit(h->hspi, rst_pattern, sizeof(rst_pattern), 100);
	if (status != HAL_OK) {
		h->lastSpiErrorCode = h->hspi->ErrorCode;
		CS_High(h);
		return status;
	}
    HAL_Delay(100);   /* hold CS low after transmitting */
    CS_High(h);
    HAL_SPIEx_FlushRxFifo(h->hspi);
    HAL_Delay(500);

	return HAL_OK;
}

/* --------------------------------------------------------------------------
 * MCP3464_StartContinuous
 *
 * Arms the first DMA TxRx of exactly 3 bytes (1 CMD + 2 ADCDATA bytes for
 * 16-bit format).  When the transfer completes, MCP3464_TxRxCpltCallback
 * re-arms immediately, creating a self-sustaining DMA loop.
 *
 * Call from the main context (not from an ISR).
 * -------------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3464_StartContinuous_DMA(MCP3464_Handle *h)
{
	if (h->running) {
		return HAL_OK;
	}

    h->running = 1;

    /* Prepare the static-read command — stays constant for all reads */
    h->txBuf[0] = MCP3464_CMD(MCP3464_REG_ADCDATA, MCP3464_CMD_SREAD);

    uint8_t spi_size = is_scan_mode(h) ? 5 : 3;
    for (int i = 1; i < spi_size; i++) h->txBuf[i] = 0x00;

    /*
     * Don't fire a DMA transfer here. The EXTI callback on the IRQ pin
     * will call MCP3464_ArmRead() when a conversion is actually ready.
     * This avoids wasted SPI transfers polling for stale data.
     */
	if ((h->drdyPort != NULL) && (h->drdyPin != 0U)) {
		h->lastDrdyLevel = (uint8_t)HAL_GPIO_ReadPin(h->drdyPort, h->drdyPin);
		if (h->lastDrdyLevel == GPIO_PIN_RESET) {
			h->startKickCount++;
			return MCP3464_ArmRead(h);
		}
	}

	return HAL_OK;
}

/* --------------------------------------------------------------------------
 * MCP3464_ArmRead
 *
 * Fires a single DMA read of ADCDATA. Called from HAL_GPIO_EXTI_Callback
 * when the MCP3464 IRQ pin signals a new conversion is ready.
 *
 * Must be safe to call from ISR context.
 * -------------------------------------------------------------------------- */
HAL_StatusTypeDef MCP3464_ArmRead(MCP3464_Handle *h)
{
	h->armAttemptCount++;

	if ((h->drdyPort != NULL) && (h->drdyPin != 0U)) {
		h->lastDrdyLevel = (uint8_t)HAL_GPIO_ReadPin(h->drdyPort, h->drdyPin);
	}

	if (!h->running) {
		h->droppedStoppedCount++;
		return HAL_BUSY;
	}

	if (h->transferPending) {
		h->droppedBusyCount++;
		return HAL_BUSY;
	}

    uint8_t spi_size = is_scan_mode(h) ? 5 : 3;
	HAL_StatusTypeDef status;

	h->armAcceptedCount++;
    h->transferPending = 1;
    CS_Low(h);
	status = HAL_SPI_TransmitReceive_DMA(h->hspi, h->txBuf, h->rxBuf, spi_size);
	h->lastStartStatus = (uint8_t)status;

	if (status == HAL_OK) {
		h->dmaStartOkCount++;
		return HAL_OK;
	}

	h->dmaStartFailCount++;
	h->lastSpiErrorCode = h->hspi->ErrorCode;
	h->transferPending = 0U;
	CS_High(h);

	return status;
}


HAL_StatusTypeDef MCP3464_SetMode(MCP3464_Handle *h, MCP3464_Mode mode, uint8_t mux, MCP3464_Osr osr)
{
	HAL_StatusTypeDef status;

	/* Shared config — same for all modes */
	    h->config.reg.config0.VREF_SEL   = VREF_EXT;
	    h->config.reg.config0.PARTIAL_SD = 1;
	    h->config.reg.config0.CLK_SEL    = CLK_EXT_DEF;
	    h->config.reg.config0.CS_SEL     = CS_SEL_NONE;
	    h->config.reg.config0.ADC_MODE   = ADC_MODE_CONVERSION;

	    h->config.reg.config1.PRE     	 = PRE_DIV1;
	    h->config.reg.config1.OSR 		 = osr;
	    h->config.reg.config1.RESERVED	 = 0;

	    h->config.reg.config2.BOOST    = BOOST_X1;
	    h->config.reg.config2.GAIN     = GAIN_X1;
	    h->config.reg.config2.AZ_MUX   = AZ_MUX_DIS;
	    h->config.reg.config2.AZ_REF   = MCP_ENABLE;
	    h->config.reg.config2.RESERVED = 1;

	    h->config.reg.config3.EN_GAINCAL = MCP_DISABLE;
	    h->config.reg.config3.EN_OFFCAL  = MCP_DISABLE;
	    h->config.reg.config3.EN_CRCCOM  = MCP_DISABLE;
	    h->config.reg.config3.CRC_FORMAT = CRC_16BIT;
	    h->config.reg.config3.CONV_MODE  = CONV_CONTINUOUS;

	    h->config.reg.irq.IRQ_MODE1  = IRQ_OUT_IRQ;
	    h->config.reg.irq.IRQ_MODE0  = IRQ_PIN_HIGH_Z;
	    h->config.reg.irq.EN_FASTCMD = MCP_ENABLE;
	    h->config.reg.irq.EN_STP     = MCP_ENABLE;

	    switch (mode) {
	    case MCP3464_MODE_SCAN:
	        h->config.reg.config3.DATA_FORMAT = FORMAT_32_CHID;
	        h->config.reg.mux.MUX_VIN_POS    = MUX_CH0;
	        h->config.reg.mux.MUX_VIN_NEG    = MUX_AGND;
	        h->scan.SE_CH    = 0xFF;
	        h->scan.DIFF_CH  = 0;
	        h->scan.DLY      = 0;
	        h->scan.RESERVED = 0;
	        h->scan.UNIMPL   = 0;
	        h->scan.TEMP     = 0;
	        h->scan.AVDD     = 0;
	        h->scan.VCM      = 0;
	        h->scan.OFFSET   = 0;
	        break;

	    case MCP3464_MODE_DIFF:
	        h->config.reg.config3.DATA_FORMAT = FORMAT_16BIT;
	        h->config.reg.mux.MUX_VIN_POS    = (mux >> 4) & 0x0F;
	        h->config.reg.mux.MUX_VIN_NEG    = mux & 0x0F;
	        h->scan.SE_CH = 0x00;
	        h->scan.DLY   = 0;
	        break;

	    case MCP3464_MODE_SINGLE:
	    default:
	        h->config.reg.config3.DATA_FORMAT = FORMAT_16BIT;
	        h->config.reg.mux.MUX_VIN_POS    = (mux >> 4) & 0x0F;
	        h->config.reg.mux.MUX_VIN_NEG    = mux & 0x0F;
	        h->scan.SE_CH = 0x00;
	        h->scan.DLY   = 0;
	        break;
	    }

	    /* Build and send 10-byte packet */
	    uint8_t pkt[10];
	    pkt[0] = MCP3464_CMD(MCP3464_REG_CONFIG0, MCP3464_CMD_IWRITE);
	    pkt[1] = build_config0(h);
	    pkt[2] = build_config1(h);
	    pkt[3] = build_config2(h);
	    pkt[4] = build_config3(h);
	    pkt[5] = build_irq(h);
	    pkt[6] = build_mux(h);
	    pkt[7] = (h->scan.DLY << 5);
	    pkt[8] = 0x00U;
	    pkt[9] = h->scan.SE_CH;

	    CS_Low(h);
	    status = HAL_SPI_Transmit(h->hspi, pkt, sizeof(pkt), 100);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	CS_High(h);
	    	return status;
	    }
	    CS_High(h);
	    HAL_SPIEx_FlushRxFifo(h->hspi);

	    uint8_t start = MCP3464_FCMD_START;
	    CS_Low(h);
	    status = HAL_SPI_Transmit(h->hspi, &start, 1, 10);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	CS_High(h);
	    	return status;
	    }
	    CS_High(h);
	    HAL_SPIEx_FlushRxFifo(h->hspi);
	    HAL_Delay(10);

	    /* Debug readbacks */
	    dbg_tx0[0] = MCP3464_CMD(MCP3464_REG_CONFIG0, MCP3464_CMD_SREAD); dbg_tx0[1] = 0x00;
	    CS_Low(h);
	    status = HAL_SPI_TransmitReceive(h->hspi, dbg_tx0, dbg_rx0, 2, 10);
	    CS_High(h);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	return status;
	    }

	    dbg_tx1[0] = MCP3464_CMD(MCP3464_REG_CONFIG3, MCP3464_CMD_SREAD); dbg_tx1[1] = 0x00;
	    CS_Low(h);
	    status = HAL_SPI_TransmitReceive(h->hspi, dbg_tx1, dbg_rx1, 2, 10);
	    CS_High(h);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	return status;
	    }

	    dbg_tx2[0] = MCP3464_CMD(MCP3464_REG_IRQ, MCP3464_CMD_SREAD); dbg_tx2[1] = 0x00;
	    CS_Low(h);
	    status = HAL_SPI_TransmitReceive(h->hspi, dbg_tx2, dbg_rx2, 2, 10);
	    CS_High(h);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	return status;
	    }

	    dbg_tx3[0] = MCP3464_CMD(MCP3464_REG_MUX, MCP3464_CMD_SREAD); dbg_tx3[1] = 0x00;
	    CS_Low(h);
	    status = HAL_SPI_TransmitReceive(h->hspi, dbg_tx3, dbg_rx3, 2, 10);
	    CS_High(h);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	return status;
	    }

	    dbg_adc_tx[0] = MCP3464_CMD(MCP3464_REG_ADCDATA, MCP3464_CMD_SREAD);
	    dbg_adc_tx[1] = 0x00;
	    dbg_adc_tx[2] = 0x00;
	    CS_Low(h);
	    status = HAL_SPI_TransmitReceive(h->hspi, dbg_adc_tx, dbg_adc_rx, 3, 10);
	    CS_High(h);
	    if (status != HAL_OK) {
	    	h->lastSpiErrorCode = h->hspi->ErrorCode;
	    	return status;
	    }

	    return HAL_OK;
}

/* --------------------------------------------------------------------------
 * MCP3464_Stop
 *
 * Signals the callback to not re-arm after the current transfer, then
 * blocks briefly until the in-flight DMA finishes and CS is deasserted.
 * -------------------------------------------------------------------------- */
void MCP3464_Stop(MCP3464_Handle *h)
{
    h->running = 0;

    uint32_t t0 = HAL_GetTick();
    while (h->transferPending && (HAL_GetTick() - t0 < 20U))
    {
        /* Wait for the last in-flight transfer to complete */
    }
}

/* --------------------------------------------------------------------------
 * MCP3464_TxRxCpltCallback
 *
 * Called from HAL_SPI_TxRxCpltCallback (ISR context).  Must be fast.
 *
 * Buffer layout on entry (16-bit DATA_FORMAT=00):
 *   rxBuf[0] = STATUS byte (clocked out while CMD byte was sent)
 *              bit2 DR_STATUS: 0 = new data ready, 1 = no new data
 *   rxBuf[1] = ADCDATA[15:8]  (MSB, sign bit in bit7)
 *   rxBuf[2] = ADCDATA[7:0]   (LSB)
 * -------------------------------------------------------------------------- */
void MCP3464_TxRxCpltCallback(MCP3464_Handle *h)
{
    /* De-assert CS first — required before any further SPI activity */
    CS_High(h);
	h->dmaCompleteCount++;
	h->lastRxStatus = h->rxBuf[0];
    /*
     * Only latch a new result when the device confirms fresh data.
     * DR_STATUS = 0 means the ADCDATA register was updated since the last
     * read (Section 6.2.6).  Skipping stale reads avoids presenting the
     * same sample twice to the application.
     */

	if ((h->rxBuf[0] & MCP3464_STATUS_DR_MASK) == 0U)
	{
		if (!is_scan_mode(h)) {
			int16_t raw = (int16_t)((h->rxBuf[1] << 8) | h->rxBuf[2]);
			h->result = (int32_t)raw;
			h->voltage = MCP3464_ToVoltage(raw, MCP3464_EXT_VREF_VOLTS);
		} else {
			uint8_t channel = (uint8_t)((h->rxBuf[1] >> 4) & 0x0FU);
			int16_t raw = (int16_t)((h->rxBuf[3] << 8) | h->rxBuf[4]);
			if (channel < MCP3464_SCAN_CHANNEL_COUNT) {
				h->scanResult[channel] = (int32_t)raw;
				h->scanVoltage[channel] = MCP3464_ToVoltage(raw, MCP3464_EXT_VREF_VOLTS);
				h->channelSampleCount[channel]++;
				h->lastChannel = channel;
				h->readyMask |= (uint8_t)(1U << channel);
				h->result = (int32_t)raw;
				h->voltage = h->scanVoltage[channel];
			}
		}

		h->dataReady = 1U;
		sampleCount++;
	}

	/* Release the lock — the next EXTI will trigger a new read */
	h->transferPending = 0;
}

void MCP3464_ErrorCallback(MCP3464_Handle *h)
{
	h->spiErrorCount++;
	h->lastSpiErrorCode = h->hspi->ErrorCode;
	h->transferPending = 0U;
	CS_High(h);
}

/* --------------------------------------------------------------------------
 * Accessors
 * -------------------------------------------------------------------------- */

int32_t MCP3464_GetResult(const MCP3464_Handle *h)
{
    return h->result;
}

/*
 * 16-bit two's complement: ±32768 codes = ±VREF.
 * E.g. raw = +16384, VREF = 3.0 V → 1.5 V.
 */
float MCP3464_ToVoltage(int32_t raw, float vref)
{
    return ((float)raw / 32768.0f) * vref;
}
