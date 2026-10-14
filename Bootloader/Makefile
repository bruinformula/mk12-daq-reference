################################################################################
# BFR STM32U535 DAQ CAN Bootloader Makefile
# Bruin Formula Racing (UCLA Formula SAE)
################################################################################

TARGET = bootloader
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
MCU = $(CPU) -mthumb

# C sources
C_SOURCES = \
Core/Src/main.c \
Core/Src/bootloader.c \
Core/Src/fdcan.c \
Core/Src/flash_if.c \
Core/Src/crc32.c \
Core/Src/led.c \
Core/Src/syscalls.c

# Assembly sources
ASM_SOURCES = \
Core/Src/startup_stm32u535xx.s

# C includes
C_INCLUDES = \
-ICore/Inc

# Compile flags
CFLAGS = $(MCU) $(C_INCLUDES) -Os -Wall -fdata-sections -ffunction-sections -fno-common
CFLAGS += -g -gdwarf-2

# Linker script
LDSCRIPT = STM32U535CCTX_FLASH_BOOTLOADER.ld

# Libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "  CC    $<"
	@$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo "  AS    $<"
	@$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo "  LD    $@"
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

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts."

.PHONY: all clean
