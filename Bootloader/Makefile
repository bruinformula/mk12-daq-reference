################################################################################
# BFR Dual-Target DAQ CAN Bootloader Makefile (STM32U535 & STM32L552)
# Bruin Formula Racing (UCLA Formula SAE)
################################################################################

MCU ?= STM32U535
BUILD_DIR = build

# Auto-detect ARM GNU Toolchain from STM32CubeIDE plugins or PATH
CUBE_TOOLCHAIN := $(shell find /Applications/STM32CubeIDE*.app/Contents/Eclipse/plugins -name "arm-none-eabi-gcc" 2>/dev/null | grep "aarch64" | head -n 1)
ifeq ($(CUBE_TOOLCHAIN),)
  CUBE_TOOLCHAIN := $(shell find /Applications/STM32CubeIDE*.app/Contents/Eclipse/plugins -name "arm-none-eabi-gcc" 2>/dev/null | head -n 1)
endif
ifneq ($(CUBE_TOOLCHAIN),)
  TOOLCHAIN_DIR := $(shell dirname "$(CUBE_TOOLCHAIN)")
  PREFIX = "$(TOOLCHAIN_DIR)/arm-none-eabi-"
else
  PREFIX = arm-none-eabi-
endif

CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# Target MCU Architecture Flags (Cortex-M33)
CPU = -mcpu=cortex-m33
MCU_FLAGS = $(CPU) -mthumb

# Normalize MCU selection
ifeq ($(filter $(MCU),stm32l5 STM32L5 STM32L552 stm32l552 STM32L552xx l552 L552),$(MCU))
  MCU_FAMILY = STM32L552xx
  LDSCRIPT = STM32L552CCTX_FLASH_BOOTLOADER.ld
  ASM_SOURCES = Core/Src/startup_stm32l552xx.s
  TARGET = bootloader_l5
else
  MCU_FAMILY = STM32U535xx
  LDSCRIPT = STM32U535CCTX_FLASH_BOOTLOADER.ld
  ASM_SOURCES = Core/Src/startup_stm32u535xx.s
  TARGET = bootloader_u5
endif

# C sources
C_SOURCES = \
Core/Src/main.c \
Core/Src/bootloader.c \
Core/Src/fdcan.c \
Core/Src/flash_if.c \
Core/Src/crc32.c \
Core/Src/led.c \
Core/Src/syscalls.c

# C includes
C_INCLUDES = \
-ICore/Inc

# Compile flags
CFLAGS = $(MCU_FLAGS) $(C_INCLUDES) -D$(MCU_FAMILY) -Os -Wall -fdata-sections -ffunction-sections -fno-common
CFLAGS += -g -gdwarf-2

# Libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU_FLAGS) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	@cp -f $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/bootloader.elf
	@cp -f $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/bootloader.hex
	@cp -f $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/bootloader.bin
	@echo "  Built $(TARGET) ($(MCU_FAMILY)) successfully."

OBJECTS = $(addprefix $(BUILD_DIR)/$(TARGET)_,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/$(TARGET)_,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/$(TARGET)_%.o: %.c Makefile | $(BUILD_DIR)
	@echo "  CC [$(MCU_FAMILY)] $<"
	@$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET)_%.o: %.s Makefile | $(BUILD_DIR)
	@echo "  AS [$(MCU_FAMILY)] $<"
	@$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	@echo "  LD [$(MCU_FAMILY)] $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "  HEX   $@"
	@$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "  BIN   $@"
	@$(BIN) $< $@

$(BUILD_DIR):
	@mkdir -p $@

all-targets:
	@$(MAKE) clean
	@$(MAKE) MCU=STM32U535
	@$(MAKE) MCU=STM32L552
	@echo "All targets (STM32U535 & STM32L552) built successfully."

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts."

.PHONY: all clean all-targets
