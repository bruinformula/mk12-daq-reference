################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Charging/Src/balancing.c \
../Charging/Src/charging.c \
../Charging/Src/elcon_charger.c \
../Charging/Src/j_plug.c 

OBJS += \
./Charging/Src/balancing.o \
./Charging/Src/charging.o \
./Charging/Src/elcon_charger.o \
./Charging/Src/j_plug.o 

C_DEPS += \
./Charging/Src/balancing.d \
./Charging/Src/charging.d \
./Charging/Src/elcon_charger.d \
./Charging/Src/j_plug.d 


# Each subdirectory must supply rules for building sources it contributes
Charging/Src/%.o Charging/Src/%.su Charging/Src/%.cyclo: ../Charging/Src/%.c Charging/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Charging/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/program/inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/lib/inc" -I../Core/Inc -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Calculations/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Runtime/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Logging/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Charging-2f-Src

clean-Charging-2f-Src:
	-$(RM) ./Charging/Src/balancing.cyclo ./Charging/Src/balancing.d ./Charging/Src/balancing.o ./Charging/Src/balancing.su ./Charging/Src/charging.cyclo ./Charging/Src/charging.d ./Charging/Src/charging.o ./Charging/Src/charging.su ./Charging/Src/elcon_charger.cyclo ./Charging/Src/elcon_charger.d ./Charging/Src/elcon_charger.o ./Charging/Src/elcon_charger.su ./Charging/Src/j_plug.cyclo ./Charging/Src/j_plug.d ./Charging/Src/j_plug.o ./Charging/Src/j_plug.su

.PHONY: clean-Charging-2f-Src

