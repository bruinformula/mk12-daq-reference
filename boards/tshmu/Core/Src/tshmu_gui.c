#include "tshmu_gui.h"
#include "sh1106.h"
#include <stdio.h>
#include <string.h>

#define SCREEN_REFRESH_MS 100U

static uint32_t last_flow_time = 0;
static uint32_t last_display_refresh = 0;

extern volatile float therm_temperatures[6];
extern uint8_t TSHMU_BOARD_ID;

void UI_InitDashboard(void) {
  SH1106_Clear();
}

void UI_UpdateRenderCycles(float flow1, uint16_t raw1, float flow2, uint16_t raw2, uint32_t now) {
  if ((now - last_flow_time) >= SCREEN_REFRESH_MS) {
    last_flow_time = now;
    
    char buf[48];
    
    // Flow 1
    int f1_int = (int)flow1;
    int f1_frac = (int)((flow1 - f1_int) * 10);
    if (f1_frac < 0) f1_frac = -f1_frac;
    snprintf(buf, sizeof(buf), "F1: %d.%dLPM (%u)      ", f1_int, f1_frac, (unsigned int)raw1);
    SH1106_GotoXY(2, 0);
    SH1106_Puts(buf, &Font_5x8, 1);
    
    // Flow 2
    int f2_int = (int)flow2;
    int f2_frac = (int)((flow2 - f2_int) * 10);
    if (f2_frac < 0) f2_frac = -f2_frac;
    snprintf(buf, sizeof(buf), "F2: %d.%dLPM (%u)      ", f2_int, f2_frac, (unsigned int)raw2);
    SH1106_GotoXY(2, 10);
    SH1106_Puts(buf, &Font_5x8, 1);

    // Thermistors 1-2
    int t1_int = (int)therm_temperatures[0];
    int t1_frac = (int)((therm_temperatures[0] - t1_int) * 100);
    if (t1_frac < 0) t1_frac = -t1_frac;

    int t2_int = (int)therm_temperatures[1];
    int t2_frac = (int)((therm_temperatures[1] - t2_int) * 100);
    if (t2_frac < 0) t2_frac = -t2_frac;

    snprintf(buf, sizeof(buf), "T1:%2d.%02d T2:%2d.%02d  ", 
             t1_int, t1_frac, t2_int, t2_frac);
    SH1106_GotoXY(2, 20);
    SH1106_Puts(buf, &Font_5x8, 1);

    // Thermistors 3-4
    int t3_int = (int)therm_temperatures[2];
    int t3_frac = (int)((therm_temperatures[2] - t3_int) * 100);
    if (t3_frac < 0) t3_frac = -t3_frac;

    int t4_int = (int)therm_temperatures[3];
    int t4_frac = (int)((therm_temperatures[3] - t4_int) * 100);
    if (t4_frac < 0) t4_frac = -t4_frac;

    snprintf(buf, sizeof(buf), "T3:%2d.%02d T4:%2d.%02d  ", 
             t3_int, t3_frac, t4_int, t4_frac);
    SH1106_GotoXY(2, 30);
    SH1106_Puts(buf, &Font_5x8, 1);

    // Thermistors 5-6
    int t5_int = (int)therm_temperatures[4];
    int t5_frac = (int)((therm_temperatures[4] - t5_int) * 100);
    if (t5_frac < 0) t5_frac = -t5_frac;

    int t6_int = (int)therm_temperatures[5];
    int t6_frac = (int)((therm_temperatures[5] - t6_int) * 100);
    if (t6_frac < 0) t6_frac = -t6_frac;

    snprintf(buf, sizeof(buf), "T5:%2d.%02d T6:%2d.%02d  ", 
             t5_int, t5_frac, t6_int, t6_frac);
    SH1106_GotoXY(2, 40);
    SH1106_Puts(buf, &Font_5x8, 1);

    // Footer with Board ID
    snprintf(buf, sizeof(buf), "TSHMU B%u Live        ", TSHMU_BOARD_ID);
    SH1106_GotoXY(2, 50);
    SH1106_Puts(buf, &Font_5x8, 1);
  }

  // DMA Display Refresh pacing (10Hz / 100ms)
  if ((now - last_display_refresh) >= SCREEN_REFRESH_MS) {
    last_display_refresh = now;
    SH1106_UpdateScreenDMA();
  }
}

void UI_HandleI2C_MasterTxCplt(I2C_HandleTypeDef *hi2c) {
  if (SH1106_HandleMasterTxCplt(hi2c)) {
    return;
  }
}

