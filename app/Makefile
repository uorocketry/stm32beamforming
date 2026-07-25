BUILD_DIR := build
PROJECT := stm32beamforming
DEVICE := stm32f072r8t6

OPENCM3_DIR := ../libopencm3
OPENCM3_DEFS := -DSTM32F0

CFILES := \
	src/main.c \
	src/beamforming_protocol.c \
	src/build_info.c \
	src/can_bus.c \
	src/can_control.c \
	src/can_filter.c \
	src/can_protocol.c \
	src/can_queue.c \
	src/can_runtime.c \
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
CAN_NODE_ID ?= 1
CPPFLAGS += -DFIRMWARE_REVISION=\"$(GIT_REVISION)\"
CPPFLAGS += -DBEAMFORMER_NODE_ID=$(CAN_NODE_ID)
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
