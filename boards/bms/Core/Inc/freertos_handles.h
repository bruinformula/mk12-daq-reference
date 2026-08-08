/*
 * freertos_handles.h
 *
 *  Created on: Apr 10, 2026
 *      Author: ishanchitale
 */

#ifndef INC_FREERTOS_HANDLES_H_
#define INC_FREERTOS_HANDLES_H_

#include "cmsis_os.h"

extern osThreadId voltageTaskHandle;
extern osThreadId tempTaskHandle;
extern osThreadId safetyTaskHandle;
extern osThreadId currTaskHandle;
extern osThreadId dataloggingTaskHandle;
extern osThreadId prchgTaskHandle;
extern osThreadId currLimitTaskHandle;
extern osThreadId balancingTaskHandle;
extern osThreadId chargingTaskHandle;
extern osThreadId socTaskHandle;
extern osThreadId serialCmdTaskHandle;
extern osMutexId SPI_MUTEXHandle;
extern osMutexId CAN_MUTEXHandle;
extern osMutexId VOLTAGE_MUTEXHandle;
extern osMutexId TEMP_MUTEXHandle;
extern osMutexId FAULT_MUTEXHandle;

#endif /* INC_FREERTOS_HANDLES_H_ */
