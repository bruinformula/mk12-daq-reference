/**
 * @file    led.h
 * @brief   BFR Status LED Driver (PB7 -> Q4 NPN -> D18 Red LED)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_LED_H_ */
