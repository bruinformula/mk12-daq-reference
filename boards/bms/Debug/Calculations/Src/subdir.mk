################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Calculations/Src/current_calculations.c \
../Calculations/Src/ocv_table.c \
../Calculations/Src/state_of_charge.c \
../Calculations/Src/thermistor.c \
../Calculations/Src/voltage_calculations.c 

OBJS += \
./Calculations/Src/current_calculations.o \
./Calculations/Src/ocv_table.o \
./Calculations/Src/state_of_charge.o \
./Calculations/Src/thermistor.o \
./Calculations/Src/voltage_calculations.o 

C_DEPS += \
./Calculations/Src/current_calculations.d \
./Calculations/Src/ocv_table.d \
./Calculations/Src/state_of_charge.d \
./Calculations/Src/thermistor.d \
./Calculations/Src/voltage_calculations.d 


# Each subdirectory must supply rules for building sources it contributes
Calculations/Src/%.o Calculations/Src/%.su Calculations/Src/%.cyclo: ../Calculations/Src/%.c Calculations/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Charging/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/program/inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/lib/inc" -I../Core/Inc -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Calculations/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Runtime/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Logging/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Calculations-2f-Src

clean-Calculations-2f-Src:
	-$(RM) ./Calculations/Src/current_calculations.cyclo ./Calculations/Src/current_calculations.d ./Calculations/Src/current_calculations.o ./Calculations/Src/current_calculations.su ./Calculations/Src/ocv_table.cyclo ./Calculations/Src/ocv_table.d ./Calculations/Src/ocv_table.o ./Calculations/Src/ocv_table.su ./Calculations/Src/state_of_charge.cyclo ./Calculations/Src/state_of_charge.d ./Calculations/Src/state_of_charge.o ./Calculations/Src/state_of_charge.su ./Calculations/Src/thermistor.cyclo ./Calculations/Src/thermistor.d ./Calculations/Src/thermistor.o ./Calculations/Src/thermistor.su ./Calculations/Src/voltage_calculations.cyclo ./Calculations/Src/voltage_calculations.d ./Calculations/Src/voltage_calculations.o ./Calculations/Src/voltage_calculations.su

.PHONY: clean-Calculations-2f-Src

