# Mk.11 High Voltage BMS

Firmware for the Mk.11 High Voltage Battery Management System. Runs on an STM32 G474RE leveraging FreeRTOS and the ADBMS6830 IC Library.

## FEATURES

- Reads cell voltage and auxiliary measurements from ADBMS6830 battery monitor ICs.
- Computes pack voltage, temperatures, current, state of charge, and fault conditions.
- Manages BMS operating states including idle, GUI wait, charging, balancing, precharge, drive, and fault states.
- Controls charging behavior for an Elcon charger and J1772 plug interface.
- Performs cell balancing through the ADBMS6830 PWM discharge controls.
- Publishes BMS telemetry over CAN and can optionally emit JSON-style GUI/debug output.

## CODE LAYOUT

```text
Core/          STM32Cube-generated startup, peripherals, FreeRTOS tasks, BMS state and safety logic
ADBMS6830/     ADBMS6830 driver and application wrapper code
Calculations/  Voltage, current, thermistor, OCV Table, and State-of-Charge calculations
Charging/      Charging, Charger CAN messages, J1772 Handling, and Balancing logic
Logging/       CAN datalogging and GUI Interaction/Serial Telemetry functions
Drivers/       STM32 HAL and CMSIS device support
Middlewares/   FreeRTOS middleware
Runtime/       Precharge Sequence and Current Limiting for running the car
```

## QUICK CONFIGURATION

High-level BMS behavior is configured in `Core/Inc/bms_state.h`, including datalogging and fault-enable flags:

- `BMS_JSON_DATALOGGING_MODE`
- `BMS_CAN_DATALOGGING_MODE`
- `BMS_FAULT_OVERVOLTAGE`
- `BMS_FAULT_UNDERVOLTAGE`
- `BMS_FAULT_OVERTEMP`
- `BMS_FAULT_UNDERTEMP`
- `BMS_FAULT_OVERCURRENT`
- `BMS_FAULT_IC_DISCONNECT`