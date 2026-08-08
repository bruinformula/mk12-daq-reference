################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drive_Loop/Src/curr_limiting.c \
../Drive_Loop/Src/prchg.c 

OBJS += \
./Drive_Loop/Src/curr_limiting.o \
./Drive_Loop/Src/prchg.o 

C_DEPS += \
./Drive_Loop/Src/curr_limiting.d \
./Drive_Loop/Src/prchg.d 


# Each subdirectory must supply rules for building sources it contributes
Drive_Loop/Src/%.o Drive_Loop/Src/%.su Drive_Loop/Src/%.cyclo: ../Drive_Loop/Src/%.c Drive_Loop/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Charging/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/program/inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/ADBMS6830/lib/inc" -I../Core/Inc -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Calculations/Inc" -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/Drive_Loop/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I"/Users/ishanchitale/STM32CubeIDE/workspace_1.16.0/mk11-bms-mcu/GUI/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drive_Loop-2f-Src

clean-Drive_Loop-2f-Src:
	-$(RM) ./Drive_Loop/Src/curr_limiting.cyclo ./Drive_Loop/Src/curr_limiting.d ./Drive_Loop/Src/curr_limiting.o ./Drive_Loop/Src/curr_limiting.su ./Drive_Loop/Src/prchg.cyclo ./Drive_Loop/Src/prchg.d ./Drive_Loop/Src/prchg.o ./Drive_Loop/Src/prchg.su

.PHONY: clean-Drive_Loop-2f-Src

