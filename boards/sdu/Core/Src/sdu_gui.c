#include "sdu_gui.h"
#include <stdio.h>
#include <string.h>

extern bool mlx90640_detected;

/* --- Private Display Refresh Timers --- */
static uint32_t last_btemp_time = 0;
static uint32_t last_ttemp_time = 0;
static uint32_t last_sg_time = 0;
static uint32_t last_display_refresh = 0;

/* --- Private Layout Drawing Widgets --- */

static void DisplayMcpScanVoltages(const SDU_State_t *state) {
  static const uint8_t sgChannels[] = {0U, 1U, 2U, 3U, 4U, 5U};
  const uint32_t numChannels = sizeof(sgChannels) / sizeof(sgChannels[0]);

  float scanVoltages[numChannels];
  char line[2][32];
  uint16_t tokenStartX[numChannels];
  uint16_t tokenEndX[numChannels];
  uint8_t negativeMask = 0U;
  size_t offset[2] = {0U, 0U};

  const uint16_t labelX = 2U;
  const uint16_t valueX = 20U;
  const uint16_t lineY[2] = {20U, 29U};
  const uint16_t overbarY[2] = {19U, 28U};

  for (uint32_t displayIndex = 0; displayIndex < numChannels; displayIndex++) {
    scanVoltages[displayIndex] = state->strain_gauge.voltages[sgChannels[displayIndex]];
  }

  line[0][0] = '\0';
  line[1][0] = '\0';

  for (uint32_t displayIndex = 0; displayIndex < numChannels; displayIndex++) {
    uint32_t row = displayIndex / STRAIN_GAUGE_VALUES_PER_ROW;
    float value = scanVoltages[displayIndex];
    char valueText[12];
    int written;
    size_t appended;

    if (value < 0.0f) {
      negativeMask |= (uint8_t)(1U << displayIndex);
      value = -value;
    }

    if (((displayIndex % STRAIN_GAUGE_VALUES_PER_ROW) > 0U) &&
        (offset[row] < (sizeof(line[row]) - 1U))) {
      line[row][offset[row]++] = '|';
      line[row][offset[row]] = '\0';
    }

    tokenStartX[displayIndex] = valueX + (uint16_t)(offset[row] * 6U);
    snprintf(valueText, sizeof(valueText), "%.3g", value);

    if (offset[row] >= (sizeof(line[row]) - 1U)) {
      break;
    }

    written = snprintf(&line[row][offset[row]], sizeof(line[row]) - offset[row],
                       "%s", valueText);
    if (written < 0) {
      break;
    }

    appended = (written < (int)(sizeof(line[row]) - offset[row]))
                   ? (size_t)written
                   : (sizeof(line[row]) - offset[row] - 1U);
    if (appended == 0U) {
      break;
    }

    tokenEndX[displayIndex] =
        tokenStartX[displayIndex] + (uint16_t)(appended * 6U) - 2U;
    offset[row] += appended;
    if (offset[row] >= sizeof(line[row])) {
      offset[row] = sizeof(line[row]) - 1U;
      line[row][offset[row]] = '\0';
      break;
    }
  }

  SH1106_DrawFilledRectangle(0, overbarY[0], SH1106_WIDTH - 1, 17,
                             SH1106_COLOR_BLACK);

  SH1106_GotoXY(labelX, lineY[0]);
  SH1106_Puts("SG:", &Font_5x8, SH1106_COLOR_WHITE);

  for (uint32_t row = 0; row < 2U; row++) {
    SH1106_GotoXY(valueX, lineY[row]);
    SH1106_Puts(line[row], &Font_5x8, SH1106_COLOR_WHITE);
  }

  for (uint32_t displayIndex = 0;
       displayIndex < (sizeof(sgChannels) / sizeof(sgChannels[0]));
       displayIndex++) {
    if ((negativeMask & (uint8_t)(1U << displayIndex)) != 0U) {
      uint32_t row = displayIndex / STRAIN_GAUGE_VALUES_PER_ROW;
      uint16_t startX = tokenStartX[displayIndex];
      uint16_t endX = tokenEndX[displayIndex];

      if (startX >= SH1106_WIDTH) {
        continue;
      }

      if (endX >= SH1106_WIDTH) {
        endX = SH1106_WIDTH - 1U;
      }

      if (endX >= startX) {
        SH1106_DrawLine(startX, overbarY[row], endX, overbarY[row],
                        SH1106_COLOR_WHITE);
      }
    }
  }
}

static void Display_TireProfileGraph(const float *profile) {
  const int sensorWidth = 32;
  const int screenWidth = 128;
  const int graphBottom = 63;
  const int graphHeightLimit = 10;
  const int barWidth = 4;

  float minVal = profile[0];
  float maxVal = profile[0];

  SH1106_DrawFilledRectangle(0, 32, screenWidth - 1, 31, SH1106_COLOR_BLACK);

  for (int i = 1; i < sensorWidth; i++) {
    if (profile[i] < minVal)
      minVal = profile[i];
    if (profile[i] > maxVal)
      maxVal = profile[i];
  }

  float range = maxVal - minVal;
  if (range < 0.001f) {
    range = 1.0f;
  }

  for (int i = 0; i < sensorWidth; i++) {
    float normalized = (profile[i] - minVal) / range;
    int barHeight = (int)(normalized * (float)graphHeightLimit + 0.5f);
    int yPos = graphBottom - barHeight;
    int xPos = i * barWidth;

    SH1106_DrawFilledRectangle(xPos, yPos, barWidth - 1, barHeight,
                               SH1106_COLOR_WHITE);
  }
}

/* --- API Implementations --- */

void UI_InitDashboard(void) {
  SH1106_Clear();
}

void UI_UpdateRenderCycles(SDU_State_t *state) {
  uint32_t now = state->timestamp_ms;

  // 1. Render Brake Temperature Widget
  if ((now - last_btemp_time) >= SCREEN_REFRESH_MS) {
    last_btemp_time = now;
    char buf[32];
    int temp_int = (int)state->brake_thermal.brake_temp_c;
    int temp_frac = (int)((state->brake_thermal.brake_temp_c - temp_int) * 10);
    
    snprintf(buf, sizeof(buf), "Brk: %d.%dC,e=0.%d", temp_int, temp_frac,
             (int)(BRAKE_EMISSIVITY * 100));
    SH1106_GotoXY(2, 0);
    SH1106_Puts(buf, &Font_5x8, 1);
  }

  // 2. Render Ambient / Hub Speed Widget
  if ((now - last_ttemp_time) >= SCREEN_REFRESH_MS) {
    last_ttemp_time = now;
    char buf[32];
    snprintf(buf, sizeof(buf), "TTa: %dC|WHS: %dRPM",
             (int)(state->tire_thermal.ambient + 0.5f), (int)state->wheel_speed.measured_rpm);
    SH1106_GotoXY(2, 10);
    SH1106_Puts(buf, &Font_5x8, 1);
  }

  // 3. Render Strain Gauges Layout
  if ((now - last_sg_time) >= SCREEN_REFRESH_MS) {
    last_sg_time = now;
    DisplayMcpScanVoltages(state);
  }

  // 4. Render Tire Temperature Histogram & Suspension Travel
  bool update_text = false;
  if (state->tire_thermal.frame_ready) {
    Display_TireProfileGraph(state->tire_thermal.profile);
    update_text = true;
  }

  static uint32_t last_sp_time = 0;
  if (update_text || ((now - last_sp_time) >= SCREEN_REFRESH_MS)) {
    last_sp_time = now;
    char buf[32];
    if (mlx90640_detected) {
      snprintf(buf, sizeof(buf), "TTo:%d|SP:%.1fmm", 
               (int)(state->tire_thermal.max_temp + 0.5f), state->shock_pot.travel_mm);
    } else {
      snprintf(buf, sizeof(buf), "TTo:--|SP:%.1fmm", state->shock_pot.travel_mm);
    }

    if (!update_text) {
      SH1106_DrawFilledRectangle(0, 40, SH1106_WIDTH, 8, SH1106_COLOR_BLACK);
    }

    SH1106_GotoXY(2, 40);
    SH1106_Puts(buf, &Font_5x8, 1);
  }

  // 5. Render Non-blocking RX diagnostics
  if (state->network.rx_last_id == 0x100) {
    char rx_buf[32];
    snprintf(rx_buf, sizeof(rx_buf), "RX ID:100 TS:%d", state->network.rx_test_buffer[4]);
    SH1106_GotoXY(2, 50);
    SH1106_Puts(rx_buf, &Font_5x8, 1);
    
    state->network.rx_last_id = 0; // Reset receive identifier trigger
  }

  // 6. Non-blocking DMA Display Refresh pacing (10Hz / 100ms)
  if ((now - last_display_refresh) >= 100U) {
    last_display_refresh = now;
    SH1106_UpdateScreenDMA();
  }
}

/* --- ISR Callback Forwarding Vectors --- */

void UI_HandleI2C_MasterTxCplt(I2C_HandleTypeDef *hi2c) {
  if (SH1106_HandleMasterTxCplt(hi2c)) {
    return;
  }
}
