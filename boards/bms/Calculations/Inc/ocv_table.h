/*
 * ocv_table.h
 *
 *  Created on: Feb 23, 2026
 *      Author: ishanchitale
 *
 *  OCV/SOC lookup table generated from ocv_soc_lookupTable_25C.csv.
 */

#ifndef INC_OCV_TABLE_H_
#define INC_OCV_TABLE_H_

#define NUM_SOC 1000
#define NUM_TEMP 1

extern const float temp_axis[NUM_TEMP];
extern const float soc_axis[NUM_SOC];
extern const float ocv_table[NUM_TEMP][NUM_SOC];

#endif /* INC_OCV_TABLE_H_ */
