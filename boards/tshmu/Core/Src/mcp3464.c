#include "mcp3464.h"
#include "main.h"

/* --- debug readback buffers — file scope guarantees debugger visibility --- */
static uint8_t dbg_tx0[2], dbg_rx0[2];  /* CONFIG0 */
static uint8_t dbg_tx1[2], dbg_rx1[2];  /* CONFIG3 */
static uint8_t dbg_tx2[2], dbg_rx2[2];  /* IRQ     */
static uint8_t dbg_tx3[2], dbg_rx3[2];  /* MUX     */
static uint8_t dbg_adc_tx[3], dbg_adc_rx[3]; /* ADCDATA polling test */

extern uint32_t sampleCount;   /* defined in main.c */
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
 * -------------------------------------------------------------------------- */
void MCP3464_Init(MCP3464_Handle *h)
{
    h->result          = 0;
    h->voltage         = 0.0f;
    h->dataReady       = 0;
    h->running         = 0;
    h->transferPending = 0;

    CS_High(h);
    HAL_Delay(1);

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
    HAL_Delay(100);
    HAL_SPI_Transmit(h->hspi, rst_pattern, sizeof(rst_pattern), 100);
    HAL_Delay(100);
    CS_High(h);
    HAL_SPIEx_FlushRxFifo(h->hspi);
    HAL_Delay(500);
}

/* --------------------------------------------------------------------------
 * MCP3464_StartContinuous
 * -------------------------------------------------------------------------- */
void MCP3464_StartContinuous_DMA(MCP3464_Handle *h)
{
    if (h->running) return;

    h->running = 1;

    h->txBuf[0] = MCP3464_CMD(MCP3464_REG_ADCDATA, MCP3464_CMD_SREAD);

    uint8_t spi_size = is_scan_mode(h) ? 5 : 3;
    for (int i = 1; i < spi_size; i++) h->txBuf[i] = 0x00;
}

/* --------------------------------------------------------------------------
 * MCP3464_ArmRead
 * -------------------------------------------------------------------------- */
void MCP3464_ArmRead(MCP3464_Handle *h)
{
    if (!h->running || h->transferPending) return;

    uint8_t spi_size = is_scan_mode(h) ? 5 : 3;

    h->transferPending = 1;
    CS_Low(h);
    HAL_SPI_TransmitReceive_DMA(h->hspi, h->txBuf, h->rxBuf, spi_size);
}

void MCP3464_SetMode(MCP3464_Handle *h, MCP3464_Mode mode, uint8_t mux, MCP3464_Osr osr)
{
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
        h->scan.SE_CH    = 0xFF; // Scan SE0 to SE7 (single-ended 0-7)
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
    HAL_SPI_Transmit(h->hspi, pkt, sizeof(pkt), 100);
    CS_High(h);
    HAL_SPIEx_FlushRxFifo(h->hspi);

    uint8_t start = MCP3464_FCMD_START;
    CS_Low(h);
    HAL_SPI_Transmit(h->hspi, &start, 1, 10);
    CS_High(h);
    HAL_SPIEx_FlushRxFifo(h->hspi);
    HAL_Delay(10);

    /* Debug readbacks */
    dbg_tx0[0] = MCP3464_CMD(MCP3464_REG_CONFIG0, MCP3464_CMD_SREAD); dbg_tx0[1] = 0x00;
    CS_Low(h); HAL_SPI_TransmitReceive(h->hspi, dbg_tx0, dbg_rx0, 2, 10); CS_High(h);

    dbg_tx1[0] = MCP3464_CMD(MCP3464_REG_CONFIG3, MCP3464_CMD_SREAD); dbg_tx1[1] = 0x00;
    CS_Low(h); HAL_SPI_TransmitReceive(h->hspi, dbg_tx1, dbg_rx1, 2, 10); CS_High(h);

    dbg_tx2[0] = MCP3464_CMD(MCP3464_REG_IRQ, MCP3464_CMD_SREAD); dbg_tx2[1] = 0x00;
    CS_Low(h); HAL_SPI_TransmitReceive(h->hspi, dbg_tx2, dbg_rx2, 2, 10); CS_High(h);

    dbg_tx3[0] = MCP3464_CMD(MCP3464_REG_MUX, MCP3464_CMD_SREAD); dbg_tx3[1] = 0x00;
    CS_Low(h); HAL_SPI_TransmitReceive(h->hspi, dbg_tx3, dbg_rx3, 2, 10); CS_High(h);

    dbg_adc_tx[0] = MCP3464_CMD(MCP3464_REG_ADCDATA, MCP3464_CMD_SREAD);
    dbg_adc_tx[1] = 0x00;
    dbg_adc_tx[2] = 0x00;
    CS_Low(h);
    HAL_SPI_TransmitReceive(h->hspi, dbg_adc_tx, dbg_adc_rx, 3, 10);
    CS_High(h);
}

void MCP3464_Stop(MCP3464_Handle *h)
{
    h->running = 0;

    uint32_t t0 = HAL_GetTick();
    while (h->transferPending && (HAL_GetTick() - t0 < 20U))
    {
        /* Wait for the last in-flight transfer to complete */
    }
}

void MCP3464_TxRxCpltCallback(MCP3464_Handle *h)
{
    CS_High(h);

    if ((h->rxBuf[0] & MCP3464_STATUS_DR_MASK) == 0U)
    {
        if (!is_scan_mode(h)) {
            int16_t raw = (int16_t)((h->rxBuf[1] << 8) | h->rxBuf[2]);
            h->result = (int32_t)raw;
            h->voltage = (float)raw * (3.0f / 32768.0f);
        }

        h->dataReady = 1U;
        sampleCount++;
    }

    h->transferPending = 0;
}

int32_t MCP3464_GetResult(const MCP3464_Handle *h)
{
    return h->result;
}

float MCP3464_ToVoltage(int32_t raw, float vref)
{
    return ((float)raw / 32768.0f) * vref;
}
