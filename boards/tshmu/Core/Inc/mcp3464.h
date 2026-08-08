#ifndef INC_MCP3464_H_
#define INC_MCP3464_H_

#include "stm32l5xx_hal.h"

/* -----------------------------------------------------------------------
 * MCP3464 – 24-bit delta-sigma ADC, SPI interface
 * ----------------------------------------------------------------------- */

/* Device hardware address (A1:A0 pins) */
#define MCP3464_ADDR              0x01U

/* Register addresses */
#define MCP3464_REG_ADCDATA       0x00U
#define MCP3464_REG_CONFIG0       0x01U
#define MCP3464_REG_CONFIG1       0x02U
#define MCP3464_REG_CONFIG2       0x03U
#define MCP3464_REG_CONFIG3       0x04U
#define MCP3464_REG_IRQ           0x05U
#define MCP3464_REG_MUX           0x06U
#define MCP3464_REG_SCAN          0x07U
#define MCP3464_REG_LOCK          0x0DU

/*
 * Command byte: [DEV_ADDR(7:6) | REG_ADDR(5:2) | CMD_TYPE(1:0)]
 * Datasheet Section 6.2, Table 6-1.
 */
#define MCP3464_CMD(reg, type)    (uint8_t)(((MCP3464_ADDR) << 6) | ((reg) << 2) | (type))

/* CMD type fields */
#define MCP3464_CMD_SREAD         0x01U   /* Static read            */
#define MCP3464_CMD_IWRITE        0x02U   /* Incremental write      */
#define MCP3464_CMD_IREAD         0x03U   /* Incremental read       */

/* Full-reset fast command */
#define MCP3464_FCMD_RESET        (uint8_t)(((MCP3464_ADDR) << 6) | 0x38U)
#define MCP3464_FCMD_START   	  (uint8_t)(((MCP3464_ADDR) << 6) | 0x28U)


#define MCP3464_STATUS_DR_MASK    0x40U   /* Mask for DR_STATUS bit (bit 6) */
/* Fast command: ADC conversion start/restart — overwrites ADC_MODE to 11 */


/* MUX Presets — Single-Ended: CHx(+) vs AGND(-) */
#define MCP3464_MUX_CH0_AGND      0x08U  /* CH0(+) vs AGND(-)  */
#define MCP3464_MUX_CH1_AGND      0x18U  /* CH1(+) vs AGND(-)  */
#define MCP3464_MUX_CH2_AGND      0x28U  /* CH2(+) vs AGND(-)  */
#define MCP3464_MUX_CH3_AGND      0x38U  /* CH3(+) vs AGND(-)  */
#define MCP3464_MUX_CH4_AGND      0x48U  /* CH4(+) vs AGND(-)  */
#define MCP3464_MUX_CH5_AGND      0x58U  /* CH5(+) vs AGND(-)  */
#define MCP3464_MUX_CH6_AGND      0x68U  /* CH6(+) vs AGND(-)  */
#define MCP3464_MUX_CH7_AGND      0x78U  /* CH7(+) vs AGND(-)  */

/* Differential MUX pairs */
#define MCP3464_MUX_DIFF_CH0_CH1  0x01U  /* CH0(+) vs CH1(-)   */
#define MCP3464_MUX_DIFF_CH2_CH3  0x23U  /* CH2(+) vs CH3(-)   */
#define MCP3464_MUX_DIFF_CH4_CH5  0x45U  /* CH4(+) vs CH5(-)   */
#define MCP3464_MUX_DIFF_CH6_CH7  0x67U  /* CH6(+) vs CH7(-)   */

// SCAN: Enable SE0 through SE7 (Single-Ended 0-7)

/* -----------------------------------------------------------------------
 * CONFIG0 — ADC mode / clock / bias
 * ----------------------------------------------------------------------- */
typedef enum {
    ADC_MODE_CONVERSION  = 0b11,  /* Continuous conversion         */
    ADC_MODE_STANDBY     = 0b10,  /* Standby (low power, fast wake)*/
    ADC_MODE_SHUTDOWN    = 0b01,  /* Full shutdown                 */
    ADC_MODE_DEFAULT     = 0b00   /* Default (shutdown)            */
} MCP3464_AdcMode;

typedef enum {
    CS_SEL_15uA   = 0b11,  /* 15 uA burnout current  */
    CS_SEL_3R7uA  = 0b10,  /* 3.7 uA                 */
    CS_SEL_0R9uA  = 0b01,  /* 0.9 uA                 */
    CS_SEL_NONE   = 0b00   /* No bias current (default) */
} MCP3464_CsSel;

typedef enum {
    CLK_INT_OUT_EN = 0b11,  /* Internal clock + AMCLK output on pin */
    CLK_INT_ONLY   = 0b10,  /* Internal clock, no output (default for this board) */
    CLK_EXT        = 0b01,  /* External digital clock  */
    CLK_EXT_DEF    = 0b00   /* External digital clock (POR default) */
} MCP3464_ClkSel;

typedef enum {
    VREF_INT = 0b1,  /* Internal VREF selected and enabled  */
    VREF_EXT = 0b0   /* External VREF via REFIN+/- (this board) */
} MCP3464_VrefSel;

/* -----------------------------------------------------------------------
 * CONFIG1 — oversampling / prescaler
 * ----------------------------------------------------------------------- */
typedef enum {
    OSR_98304 = 0b1111,
    OSR_81920 = 0b1110,
    OSR_49152 = 0b1101,
    OSR_40960 = 0b1100,
    OSR_24576 = 0b1011,
    OSR_20480 = 0b1010,
    OSR_16384 = 0b1001,
    OSR_8192  = 0b1000,
    OSR_4096  = 0b0111,	//300Hz
    OSR_2048  = 0b0110,	//150Hz <- set to this
    OSR_1024  = 0b0101,
    OSR_512   = 0b0100,
    OSR_256   = 0b0011,   /* Default */
    OSR_128   = 0b0010,
    OSR_64    = 0b0001,
    OSR_32    = 0b0000
} MCP3464_Osr;

typedef enum {
    PRE_DIV8 = 0b11,
    PRE_DIV4 = 0b10,
    PRE_DIV2 = 0b01,
    PRE_DIV1 = 0b00    /* Default: AMCLK = MCLK */
} MCP3464_Pre;

/* -----------------------------------------------------------------------
 * CONFIG2 — gain / boost / auto-zero
 * ----------------------------------------------------------------------- */
typedef enum {
    GAIN_X64  = 0b111,
    GAIN_X32  = 0b110,
    GAIN_X16  = 0b101,
    GAIN_X8   = 0b100,
    GAIN_X4   = 0b011,
    GAIN_X2   = 0b010,
    GAIN_X1   = 0b001,   /* Default */
    GAIN_X1_3 = 0b000    /* 1/3x gain */
} MCP3464_Gain;

typedef enum {
    BOOST_X2   = 0b11,  /* 2x bias current (high-speed sampling)  */
    BOOST_X1   = 0b10,  /* 1x (default)                           */
    BOOST_X0R6 = 0b01,  /* 0.66x (low power)                      */
    BOOST_X0R5 = 0b00   /* 0.5x (lowest power)                    */
} MCP3464_Boost;

/*
 * AZ_MUX: Analog input multiplexer auto-zeroing
 *   IMPORTANT: AZ_MUX_EN doubles conversion time and disables
 *   continuous mode (replaced by consecutive one-shot conversions).
 *   Must be DISABLED for continuous/DMA operation.
 */
typedef enum {
    AZ_MUX_DIS = 0,  /* Disabled (default, required for DMA) */
    AZ_MUX_EN  = 1   /* Enabled (no continuous mode)         */
} MCP3464_AzMux;

/* -----------------------------------------------------------------------
 * CONFIG3 — conversion mode / data format
 * ----------------------------------------------------------------------- */
typedef enum {
    CONV_CONTINUOUS     = 0b11,  /* Continuous conversion         */
    CONV_ONESHOT_STBY   = 0b10,  /* One-shot, then standby       */
    CONV_ONESHOT_SHD    = 0b00   /* One-shot, then shutdown (default) */
} MCP3464_ConvMode;

/*
 * DATA_FORMAT for MCP3464 (16-bit device):
 *   00 = 16-bit: [DATA15:0]
 *   01 = 32-bit left-justified: [DATA15:0][0x0000]
 *   10 = 32-bit sign-extended:  [SGN_EXT16][DATA15:0]
 *   11 = 32-bit with channel ID: [CHID4][SGN_EXT4][SGN_EXT8][DATA15:0]
 */
typedef enum {
    FORMAT_16BIT     = 0b00,  /* 16-bit data only (default)    */
    FORMAT_32_LEFT   = 0b01,  /* 32-bit left-justified         */
    FORMAT_32_SGN    = 0b10,  /* 32-bit sign-extended          */
    FORMAT_32_CHID   = 0b11   /* 32-bit with channel ID (scan) */
} MCP3464_DataFormat;

typedef enum {
    CRC_16BIT = 0,  /* Standard 16-bit CRC           */
    CRC_32BIT = 1   /* 16-bit CRC + 16 zeros         */
} MCP3464_CrcFormat;

#define MCP_ENABLE   1
#define MCP_DISABLE  0

/* -----------------------------------------------------------------------
 * IRQ register — interrupt configuration
 * ----------------------------------------------------------------------- */
typedef enum {
    IRQ_PIN_PUSH_PULL = 1,  /* Active drive high when idle  */
    IRQ_PIN_HIGH_Z    = 0   /* Open-drain, needs pull-up    */
} MCP3464_IrqMode0;

typedef enum {
    IRQ_OUT_MDAT = 1,  /* MDAT output on IRQ pin       */
    IRQ_OUT_IRQ  = 0   /* Normal IRQ output (default)   */
} MCP3464_IrqMode1;

/* -----------------------------------------------------------------------
 * MUX — channel input selection
 * ----------------------------------------------------------------------- */
typedef enum {
    MUX_CH0         = 0b0000,
    MUX_CH1         = 0b0001,
    MUX_CH2         = 0b0010,
    MUX_CH3         = 0b0011,
    MUX_CH4         = 0b0100,
    MUX_CH5         = 0b0101,
    MUX_CH6         = 0b0110,
    MUX_CH7         = 0b0111,
    MUX_AGND        = 0b1000,
    MUX_AVDD        = 0b1001,
    MUX_RESERVED    = 0b1010,
    MUX_VREF_PLUS   = 0b1011,  /* REFIN+ */
    MUX_VREF_MINUS  = 0b1100,  /* REFIN- */
    MUX_TEMP_P      = 0b1101,  /* Internal temp diode P */
    MUX_TEMP_M      = 0b1110,  /* Internal temp diode M */
    MUX_VCM         = 0b1111   /* Internal Vcm          */
} MCP3464_MuxInput;

/* Helper to build MUX byte from VIN+ and VIN- selections */
#define MCP3464_MUX(vinp, vinn)   (uint8_t)(((vinp) << 4) | (vinn))

/* -----------------------------------------------------------------------
 * Driver mode selection
 * ----------------------------------------------------------------------- */
typedef enum {
    MCP3464_MODE_SINGLE,   /* Single-ended via MUX, 16-bit output          */
    MCP3464_MODE_DIFF,     /* Differential via MUX, 16-bit signed output   */
    MCP3464_MODE_SCAN      /* Scan all SE channels, 32-bit + channel ID    */
} MCP3464_Mode;


/* -----------------------------------------------------------------------
 * Register config union — flat byte access + named bitfields
 * ----------------------------------------------------------------------- */
typedef union {
    uint8_t bytes[7];  /* bytes[0]=CONFIG0, [1]=CONFIG1, ... [5]=IRQ, [6]=MUX */
    struct {
        /* CONFIG0 */
        struct {
            uint8_t ADC_MODE   :2;  /* bits 1:0 */
            uint8_t CS_SEL     :2;  /* bits 3:2 */
            uint8_t CLK_SEL    :2;  /* bits 5:4 */
            uint8_t PARTIAL_SD :1;  /* bit 6 — must be 1 */
            uint8_t VREF_SEL   :1;  /* bit 7 */
        } config0;

        /* CONFIG1 */
        struct {
            uint8_t RESERVED   :2;  /* bits 1:0 — must be 00 */
            uint8_t OSR        :4;  /* bits 5:2 */
            uint8_t PRE        :2;  /* bits 7:6 */
        } config1;

        /* CONFIG2 */
        struct {
            uint8_t RESERVED   :1;  /* bit 0 — must be 1 */
            uint8_t AZ_REF     :1;  /* bit 1 */
            uint8_t AZ_MUX     :1;  /* bit 2 */
            uint8_t GAIN       :3;  /* bits 5:3 */
            uint8_t BOOST      :2;  /* bits 7:6 */
        } config2;

        /* CONFIG3 */
        struct {
            uint8_t EN_GAINCAL  :1;  /* bit 0 */
            uint8_t EN_OFFCAL   :1;  /* bit 1 */
            uint8_t EN_CRCCOM   :1;  /* bit 2 */
            uint8_t CRC_FORMAT  :1;  /* bit 3 */
            uint8_t DATA_FORMAT :2;  /* bits 5:4 */
            uint8_t CONV_MODE   :2;  /* bits 7:6 */
        } config3;

        /* IRQ */
        struct {
            uint8_t EN_STP         :1;  /* bit 0 */
            uint8_t EN_FASTCMD     :1;  /* bit 1 */
            uint8_t IRQ_MODE0      :1;  /* bit 2 */
            uint8_t IRQ_MODE1      :1;  /* bit 3 */
            uint8_t POR_STATUS     :1;  /* bit 4 — read only */
            uint8_t CRCCFG_STATUS  :1;  /* bit 5 — read only */
            uint8_t DR_STATUS      :1;  /* bit 6 — read only */
            uint8_t UNIMPLEMENTED  :1;  /* bit 7 */
        } irq;

        /* MUX */
        struct {
            uint8_t MUX_VIN_NEG :4;  /* bits 3:0 */
            uint8_t MUX_VIN_POS :4;  /* bits 7:4 */
        } mux;

    } reg;
} MCP3464_Config;

/* -----------------------------------------------------------------------
 * SCAN register — 24-bit, handled separately from main config union
 * ----------------------------------------------------------------------- */
typedef struct {
    uint8_t SE_CH;        /* bits 7:0  — single-ended channels CH7:CH0    */
    uint8_t DIFF_CH  :4;  /* bits 11:8 — differential pairs               */
    uint8_t TEMP     :1;  /* bit 12                                        */
    uint8_t AVDD     :1;  /* bit 13                                        */
    uint8_t VCM      :1;  /* bit 14                                        */
    uint8_t OFFSET   :1;  /* bit 15                                        */
    uint8_t UNIMPL   :4;  /* bits 19:16                                    */
    uint8_t RESERVED :1;  /* bit 20 — must be 0                            */
    uint8_t DLY      :3;  /* bits 23:21 — delay between channels           */
} MCP3464_ScanReg;

/* -----------------------------------------------------------------------
 * Driver handle — one instance per physical device
 * ----------------------------------------------------------------------- */
typedef struct {
    GPIO_TypeDef    	*csPort;
    uint16_t         	csPin;
    SPI_HandleTypeDef   *hspi;

    MCP3464_Config   	config;   /* register shadow — bitfield + byte access */
    MCP3464_ScanReg  	scan;     /* SCAN register shadow — 3 bytes           */

    volatile int32_t 	result;          /* Latest int16_t result, sign-extended   */
    volatile float   	voltage;         /* Latest result converted to volts        */
    volatile uint8_t 	dataReady;       /* 1 = fresh result in 'result'            */
    volatile uint8_t 	running;         /* 1 = continuous DMA loop is active       */
    volatile uint8_t 	transferPending; /* 1 = a DMA transfer is currently in flight */

    /*
     * 3-byte DMA buffers for 16-bit ADCDATA reads:
     *   txBuf: [CMD]    [0x00] [0x00]
     *   rxBuf: [STATUS] [MSB ] [LSB ]
     * Must remain valid until DMA completes — store in the handle (global scope).
     */
    uint8_t txBuf[5];
    uint8_t rxBuf[5];
} MCP3464_Handle;


void    MCP3464_Init(MCP3464_Handle *h);
void    MCP3464_StartContinuous_DMA(MCP3464_Handle *h);
void    MCP3464_ArmRead(MCP3464_Handle *h);   /* Trigger one DMA read (call from EXTI) */
void    MCP3464_Stop(MCP3464_Handle *h);
void    MCP3464_TxRxCpltCallback(MCP3464_Handle *h);
int32_t MCP3464_GetResult(const MCP3464_Handle *h);
float   MCP3464_ToVoltage(int32_t raw, float vref);
void 	MCP3464_SetMode(MCP3464_Handle *h, MCP3464_Mode mode, uint8_t mux, MCP3464_Osr osr);

#endif /* INC_MCP3464_H_ */
