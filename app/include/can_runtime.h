#ifndef CAN_RUNTIME_H
#define CAN_RUNTIME_H

#include "can_control.h"
#include "diagnostic_record.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct can_runtime {
    can_control_state_t state;
    uint16_t phase_command;
    uint8_t vga_command;
    uint8_t node_id;
    firmware_clock_t clock_source;
    bool safe_lockout;
    uint16_t invalid_commands;
} can_runtime_t;

bool can_runtime_start(
    can_runtime_t *runtime,
    uint8_t node_id,
    firmware_clock_t clock_source,
    bool safe_lockout,
    const can_control_state_t *initial_state,
    uint16_t initial_phase_command,
    uint8_t initial_vga_command);
bool can_runtime_service_next(can_runtime_t *runtime);

#endif
