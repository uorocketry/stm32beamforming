#ifndef DIAGNOSTIC_RECORD_H
#define DIAGNOSTIC_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#define DIAGNOSTIC_MAGIC 0x42464d31u
#define DIAGNOSTIC_VERSION 1u
#define DIAGNOSTIC_LOCKOUT_THRESHOLD 3u

typedef enum firmware_state {
    FIRMWARE_STATE_UNINITIALIZED = 0,
    FIRMWARE_STATE_BOOTING = 1,
    FIRMWARE_STATE_CLOCK_READY = 2,
    FIRMWARE_STATE_SAFE_OUTPUTS = 3,
    FIRMWARE_STATE_HEALTHY = 4,
    FIRMWARE_STATE_SAFE_LOCKOUT = 5,
    FIRMWARE_STATE_FAULT = 6
} firmware_state_t;

typedef enum firmware_clock {
    FIRMWARE_CLOCK_UNKNOWN = 0,
    FIRMWARE_CLOCK_HSE_PLL = 1,
    FIRMWARE_CLOCK_HSI48 = 2
} firmware_clock_t;

typedef enum firmware_fault {
    FIRMWARE_FAULT_NONE = 0,
    FIRMWARE_FAULT_CLOCK_STARTUP = 1,
    FIRMWARE_FAULT_CLOCK_SECURITY = 2,
    FIRMWARE_FAULT_VGA_COMMAND = 3,
    FIRMWARE_FAULT_VGA_SPI_TIMEOUT = 4,
    FIRMWARE_FAULT_VGA_SPI_ERROR = 5,
    FIRMWARE_FAULT_PHASE_COMMAND = 6,
    FIRMWARE_FAULT_PHASE_SPI_TIMEOUT = 7,
    FIRMWARE_FAULT_PHASE_SPI_ERROR = 8,
    FIRMWARE_FAULT_HARD_FAULT = 9,
    FIRMWARE_FAULT_UNEXPECTED_INTERRUPT = 10,
    FIRMWARE_FAULT_CAN_INIT = 11
} firmware_fault_t;

typedef struct diagnostic_record {
    uint32_t magic;
    uint32_t version;
    uint32_t build_id;
    uint32_t boot_count;
    uint32_t reset_flags;
    uint32_t checksum;
    uint16_t current_fault;
    uint16_t previous_fault;
    uint16_t last_phase_command;
    uint8_t last_vga_command;
    uint8_t state;
    uint8_t clock_source;
    uint8_t consecutive_incomplete_boots;
} diagnostic_record_t;

diagnostic_record_t diagnostic_record_new(void);
diagnostic_record_t diagnostic_record_begin_boot(
    const diagnostic_record_t *previous,
    uint32_t reset_flags,
    uint32_t build_id);
bool diagnostic_record_is_valid(const diagnostic_record_t *record);
bool diagnostic_record_should_lockout(const diagnostic_record_t *record);
void diagnostic_record_set_state(diagnostic_record_t *record, firmware_state_t state);
void diagnostic_record_set_clock(diagnostic_record_t *record, firmware_clock_t clock_source);
void diagnostic_record_set_commands(
    diagnostic_record_t *record,
    uint16_t phase_command,
    uint8_t vga_command);
void diagnostic_record_set_fault(diagnostic_record_t *record, firmware_fault_t fault);
void diagnostic_record_mark_healthy(diagnostic_record_t *record);

#endif
