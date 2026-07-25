#include "can_control.h"

#include "beamforming_protocol.h"

#include <stddef.h>
#include <string.h>

static can_command_result_t append_operation(
    can_control_plan_t *plan,
    can_control_operation_type_t type,
    uint16_t command)
{
    if (plan->operation_count >= CAN_CONTROL_MAX_OPERATIONS) {
        return CAN_COMMAND_RESULT_BUSY;
    }

    plan->operations[plan->operation_count].type = type;
    plan->operations[plan->operation_count].command = command;
    ++plan->operation_count;
    return CAN_COMMAND_RESULT_OK;
}

static can_command_result_t append_phase(
    uint8_t phase_state,
    uint8_t phase_address,
    can_control_plan_t *plan)
{
    uint16_t command = 0u;
    if (!phase_command_from_state(phase_state, phase_address, &command)) {
        return CAN_COMMAND_RESULT_INVALID_PAYLOAD;
    }

    const can_command_result_t result =
        append_operation(plan, CAN_CONTROL_OPERATION_PHASE, command);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    plan->resulting_state.phase_state = phase_state;
    plan->resulting_state.phase_address = phase_address;
    return CAN_COMMAND_RESULT_OK;
}

static can_command_result_t append_vga(uint8_t attenuation_db, can_control_plan_t *plan)
{
    uint8_t command = 0u;
    if (!vga_command_from_attenuation(attenuation_db, &command)) {
        return CAN_COMMAND_RESULT_INVALID_PAYLOAD;
    }

    const can_command_result_t result =
        append_operation(plan, CAN_CONTROL_OPERATION_VGA, command);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    plan->resulting_state.attenuation_db = attenuation_db;
    return CAN_COMMAND_RESULT_OK;
}

static can_command_result_t plan_combined(
    const can_command_t *command,
    can_control_plan_t *plan)
{
    can_command_result_t result = append_vga(VGA_MAX_ATTENUATION_DB, plan);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    result = append_phase(command->phase_state, command->phase_address, plan);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    if (command->attenuation_db != VGA_MAX_ATTENUATION_DB) {
        result = append_vga(command->attenuation_db, plan);
        if (result != CAN_COMMAND_RESULT_OK) {
            return result;
        }
    }

    plan->resulting_state.attenuation_db = command->attenuation_db;
    return CAN_COMMAND_RESULT_OK;
}

static can_command_result_t plan_phase_safely(
    const can_command_t *command,
    const can_control_state_t *current_state,
    can_control_plan_t *plan)
{
    if (current_state->attenuation_db == VGA_MAX_ATTENUATION_DB) {
        return append_phase(command->phase_state, command->phase_address, plan);
    }

    can_command_result_t result = append_vga(VGA_MAX_ATTENUATION_DB, plan);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    result = append_phase(command->phase_state, command->phase_address, plan);
    if (result != CAN_COMMAND_RESULT_OK) {
        return result;
    }

    return append_vga(current_state->attenuation_db, plan);
}

can_command_result_t can_control_plan_command(
    const can_command_t *command,
    const can_control_state_t *current_state,
    can_control_plan_t *plan)
{
    if (command == NULL || current_state == NULL || plan == NULL) {
        return CAN_COMMAND_RESULT_INVALID_PAYLOAD;
    }

    memset(plan, 0, sizeof(*plan));
    plan->resulting_state = *current_state;

    switch (command->type) {
    case CAN_MESSAGE_SET_PHASE:
        return plan_phase_safely(command, current_state, plan);

    case CAN_MESSAGE_SET_VGA:
        return append_vga(command->attenuation_db, plan);

    case CAN_MESSAGE_SET_COMBINED:
        return plan_combined(command, plan);

    case CAN_MESSAGE_ENTER_SAFE: {
        can_command_result_t result = append_vga(VGA_MAX_ATTENUATION_DB, plan);
        if (result != CAN_COMMAND_RESULT_OK) {
            return result;
        }
        return append_phase(0u, command->phase_address, plan);
    }

    case CAN_MESSAGE_PING:
        return CAN_COMMAND_RESULT_OK;

    case CAN_MESSAGE_STATUS:
    case CAN_MESSAGE_ACK:
    case CAN_MESSAGE_ERROR:
    case CAN_MESSAGE_TYPE_COUNT:
        return CAN_COMMAND_RESULT_UNSUPPORTED;
    }

    return CAN_COMMAND_RESULT_UNSUPPORTED;
}
