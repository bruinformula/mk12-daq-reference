################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ADBMS6830/lib/src/adBms6830CmdList.c \
../ADBMS6830/lib/src/adBms6830GenericType.c \
../ADBMS6830/lib/src/adBms6830ParseCreate.c 

OBJS += \
./ADBMS6830/lib/src/adBms6830CmdList.o \
./ADBMS6830/lib/src/adBms6830GenericType.o \
./ADBMS6830/lib/src/adBms6830ParseCreate.o 

C_DEPS += \
./ADBMS6830/lib/src/adBms6830CmdList.d \
./ADBMS6830/lib/src/adBms6830GenericType.d \
./ADBMS6830/lib/src/adBms6830ParseCreate.d 


# Each subdirectory must supply rules for building sources it contributes
ADBMS6830/lib/src/%.o ADBMS6830/lib/src/%.su ADBMS6830/lib/src/%.cyclo: ../ADBMS6830/lib/src/%.c ADBMS6830/lib/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Charging/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/program/inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/lib/inc" -I../Core/Inc -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Calculations/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Runtime/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Logging/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ADBMS6830-2f-lib-2f-src

clean-ADBMS6830-2f-lib-2f-src:
	-$(RM) ./ADBMS6830/lib/src/adBms6830CmdList.cyclo ./ADBMS6830/lib/src/adBms6830CmdList.d ./ADBMS6830/lib/src/adBms6830CmdList.o ./ADBMS6830/lib/src/adBms6830CmdList.su ./ADBMS6830/lib/src/adBms6830GenericType.cyclo ./ADBMS6830/lib/src/adBms6830GenericType.d ./ADBMS6830/lib/src/adBms6830GenericType.o ./ADBMS6830/lib/src/adBms6830GenericType.su ./ADBMS6830/lib/src/adBms6830ParseCreate.cyclo ./ADBMS6830/lib/src/adBms6830ParseCreate.d ./ADBMS6830/lib/src/adBms6830ParseCreate.o ./ADBMS6830/lib/src/adBms6830ParseCreate.su

.PHONY: clean-ADBMS6830-2f-lib-2f-src

