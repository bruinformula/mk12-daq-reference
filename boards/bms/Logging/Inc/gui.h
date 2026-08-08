/*
 * gui_test.h
 *
 *  Created on: Apr 4, 2026
 *      Author: ishanchitale
 */

#ifndef INC_GUI_H_
#define INC_GUI_H_

#include "usart.h"
#include "current_calculations.h"
#include "thermistor.h"
#include "voltage_calculations.h"
#include "adBms_Application.h"
#include "curr_limiting.h"

#define RX_BUF_SIZE 32
extern uint8_t uart_rx_buffer[RX_BUF_SIZE];
extern char serial_cmd_buffer[RX_BUF_SIZE];
void processGUI_Cmd();
void return_to_gui_from_charging_mode();
void return_to_gui_from_balancing_mode();

extern char json_buf[4096];
int build_bms_json();
void send_bms_json(int json_len);

#endif /* INC_GUI_H_ */
