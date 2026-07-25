BUILD_DIR := build
PROJECT := stm32beamforming
DEVICE := stm32f072r8t6

OPENCM3_DIR := ../libopencm3
OPENCM3_DEFS := -DSTM32F0

CFILES := \
	src/main.c \
	src/beamforming_protocol.c \
	src/build_info.c \
	src/clock_control.c \
	src/diagnostic_record.c \
	src/diagnostics.c \
	src/faults.c \
	src/phaseShifter.c \
	src/spi_guard.c \
	src/timebase.c \
	src/vga.c \
	src/watchdog.c

INCLUDES += -Iinclude
CSTD := -std=c17
OPT ?= -Os

GIT_REVISION ?= $(shell git rev-parse --verify HEAD 2>/dev/null || printf unknown)
CPPFLAGS += -DFIRMWARE_REVISION=\"$(GIT_REVISION)\"
CFLAGS += -Wall -Wextra -Werror -Wdouble-promotion -fstack-usage
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

all: $(BUILD_DIR) $(PROJECT).elf $(PROJECT).bin
	mv $(PROJECT).elf $(BUILD_DIR)/
	mv $(PROJECT).bin $(BUILD_DIR)/
