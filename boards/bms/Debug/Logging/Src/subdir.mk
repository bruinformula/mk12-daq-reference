################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Logging/Src/can_datalogging.c \
../Logging/Src/gui.c 

OBJS += \
./Logging/Src/can_datalogging.o \
./Logging/Src/gui.o 

C_DEPS += \
./Logging/Src/can_datalogging.d \
./Logging/Src/gui.d 


# Each subdirectory must supply rules for building sources it contributes
Logging/Src/%.o Logging/Src/%.su Logging/Src/%.cyclo: ../Logging/Src/%.c Logging/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Charging/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/program/inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/lib/inc" -I../Core/Inc -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Calculations/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Runtime/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Logging/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Logging-2f-Src

clean-Logging-2f-Src:
	-$(RM) ./Logging/Src/can_datalogging.cyclo ./Logging/Src/can_datalogging.d ./Logging/Src/can_datalogging.o ./Logging/Src/can_datalogging.su ./Logging/Src/gui.cyclo ./Logging/Src/gui.d ./Logging/Src/gui.o ./Logging/Src/gui.su

.PHONY: clean-Logging-2f-Src

