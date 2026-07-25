#ifndef CAN_CONTROL_H
#define CAN_CONTROL_H

#include "can_protocol.h"

#include <stdint.h>

#define CAN_CONTROL_MAX_OPERATIONS 3u

typedef enum can_control_operation_type {
    CAN_CONTROL_OPERATION_PHASE = 0,
    CAN_CONTROL_OPERATION_VGA = 1
} can_control_operation_type_t;

typedef struct can_control_operation {
    can_control_operation_type_t type;
    uint16_t command;
} can_control_operation_t;

typedef struct can_control_state {
    uint8_t phase_state;
    uint8_t phase_address;
    uint8_t attenuation_db;
} can_control_state_t;

typedef struct can_control_plan {
    can_control_operation_t operations[CAN_CONTROL_MAX_OPERATIONS];
    uint8_t operation_count;
    can_control_state_t resulting_state;
} can_control_plan_t;

can_command_result_t can_control_plan_command(
    const can_command_t *command,
    const can_control_state_t *current_state,
    can_control_plan_t *plan);

#endif
