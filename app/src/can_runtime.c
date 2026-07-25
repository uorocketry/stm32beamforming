#include "can_runtime.h"

#include "PhaseShifter.h"
#include "Vga.h"
#include "can_bus.h"
#include "can_protocol.h"
#include "diagnostics.h"
#include "faults.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define CAN_SPI_TIMEOUT_MILLIS 2u

static void increment_invalid_commands(can_runtime_t *runtime)
{
    if (runtime->invalid_commands != UINT16_MAX) {
        ++runtime->invalid_commands;
    }
}

static firmware_fault_t phase_fault_from_status(spi_guard_status_t status)
{
    return (status == SPI_GUARD_TIMEOUT)
        ? FIRMWARE_FAULT_PHASE_SPI_TIMEOUT
        : FIRMWARE_FAULT_PHASE_SPI_ERROR;
}

static firmware_fault_t vga_fault_from_status(spi_guard_status_t status)
{
    return (status == SPI_GUARD_TIMEOUT)
        ? FIRMWARE_FAULT_VGA_SPI_TIMEOUT
        : FIRMWARE_FAULT_VGA_SPI_ERROR;
}

static void send_frame_if_possible(const can_frame_t *frame)
{
    (void)can_bus_send(frame);
}

static bool command_is_unicast(const can_command_t *command)
{
    return command->destination != CAN_NODE_BROADCAST;
}

static void send_ack(const can_runtime_t *runtime, const can_command_t *command)
{
    if (!command_is_unicast(command)) {
        return;
    }

    can_frame_t response = {0};
    if (can_protocol_make_ack(
            runtime->node_id,
            command->source,
            command->sequence,
            command->type,
            &response)) {
        send_frame_if_possible(&response);
    }
}

static void send_error_for_command(
    const can_runtime_t *runtime,
    const can_command_t *command,
    can_command_result_t result)
{
    if (!command_is_unicast(command)) {
        return;
    }

    can_frame_t response = {0};
    if (can_protocol_make_error(
            runtime->node_id,
            command->source,
            command->sequence,
            command->type,
            result,
            &response)) {
        send_frame_if_possible(&response);
    }
}

static void send_error_for_frame(
    const can_runtime_t *runtime,
    const can_frame_t *frame,
    can_decode_result_t decode_result)
{
    if (!frame->extended || frame->remote) {
        return;
    }

    can_message_type_t type = CAN_MESSAGE_TYPE_COUNT;
    uint8_t destination = 0u;
    uint8_t source = 0u;
    uint16_t sequence = 0u;
    if (!can_protocol_parse_id(
            frame->id,
            &type,
            &destination,
            &source,
            &sequence)
        || destination != runtime->node_id
        || source != CAN_NODE_CONTROLLER
        || type > CAN_MESSAGE_PING) {
        return;
    }

    can_frame_t response = {0};
    if (can_protocol_make_error(
            runtime->node_id,
            source,
            sequence,
            type,
            can_protocol_result_from_decode(decode_result),
            &response)) {
        send_frame_if_possible(&response);
    }
}

static uint8_t health_flags(const can_runtime_t *runtime)
{
    uint8_t flags = CAN_HEALTH_NONE;

    if (runtime->clock_source == FIRMWARE_CLOCK_HSI48) {
        flags |= CAN_HEALTH_CLOCK_FALLBACK;
    }
    if (can_bus_rx_dropped() != 0u) {
        flags |= CAN_HEALTH_RX_DROPPED;
    }
    if (runtime->invalid_commands != 0u) {
        flags |= CAN_HEALTH_INVALID_COMMAND;
    }
    if (runtime->safe_lockout) {
        flags |= CAN_HEALTH_SAFE_LOCKOUT;
    }
    if (can_bus_is_bus_off()) {
        flags |= CAN_HEALTH_BUS_OFF;
    }
    if (can_bus_tx_dropped() != 0u) {
        flags |= CAN_HEALTH_TX_DROPPED;
    }

    return flags;
}

static void send_status(const can_runtime_t *runtime, const can_command_t *command)
{
    if (!command_is_unicast(command)) {
        return;
    }

    const can_status_payload_t status = {
        .node_id = runtime->node_id,
        .phase_state = runtime->state.phase_state,
        .phase_address = runtime->state.phase_address,
        .attenuation_db = runtime->state.attenuation_db,
        .health_flags = health_flags(runtime),
        .rx_dropped = can_bus_rx_dropped(),
        .invalid_commands = runtime->invalid_commands,
    };

    can_frame_t response = {0};
    if (can_protocol_make_status(
            runtime->node_id,
            command->source,
            command->sequence,
            &status,
            &response)) {
        send_frame_if_possible(&response);
    }
}

static void apply_operation(can_runtime_t *runtime, const can_control_operation_t *operation)
{
    if (operation->type == CAN_CONTROL_OPERATION_VGA) {
        const uint8_t command = (uint8_t)operation->command;
        const spi_guard_status_t status = vga_write(command, CAN_SPI_TIMEOUT_MILLIS);
        if (status != SPI_GUARD_OK) {
            firmware_fail(vga_fault_from_status(status));
        }
        runtime->vga_command = command;
        return;
    }

    const spi_guard_status_t status =
        phase_shifter_write(operation->command, CAN_SPI_TIMEOUT_MILLIS);
    if (status != SPI_GUARD_OK) {
        firmware_fail(phase_fault_from_status(status));
    }
    runtime->phase_command = operation->command;
}

static void apply_plan(can_runtime_t *runtime, const can_control_plan_t *plan)
{
    for (uint8_t index = 0u; index < plan->operation_count; ++index) {
        apply_operation(runtime, &plan->operations[index]);
    }

    runtime->state = plan->resulting_state;
    diagnostics_set_commands(runtime->phase_command, runtime->vga_command);
}

bool can_runtime_start(
    can_runtime_t *runtime,
    uint8_t node_id,
    firmware_clock_t clock_source,
    bool safe_lockout,
    const can_control_state_t *initial_state,
    uint16_t initial_phase_command,
    uint8_t initial_vga_command)
{
    if (runtime == NULL
        || initial_state == NULL
        || node_id < CAN_NODE_MIN
        || node_id > CAN_NODE_MAX
        || initial_state->phase_address > CAN_PHASE_ADDRESS_MAX
        || initial_state->attenuation_db > CAN_VGA_ATTENUATION_MAX_DB) {
        return false;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->state = *initial_state;
    runtime->phase_command = initial_phase_command;
    runtime->vga_command = initial_vga_command;
    runtime->node_id = node_id;
    runtime->clock_source = clock_source;
    runtime->safe_lockout = safe_lockout;

    return can_bus_setup(node_id);
}

bool can_runtime_service_next(can_runtime_t *runtime)
{
    if (runtime == NULL) {
        return false;
    }

    can_frame_t frame = {0};
    if (!can_bus_receive(&frame)) {
        return false;
    }

    can_command_t command = {0};
    const can_decode_result_t decode_result =
        can_protocol_decode_command(&frame, runtime->node_id, &command);

    if (decode_result == CAN_DECODE_IGNORED) {
        return true;
    }
    if (decode_result != CAN_DECODE_OK) {
        increment_invalid_commands(runtime);
        send_error_for_frame(runtime, &frame, decode_result);
        return true;
    }

    if (command.type == CAN_MESSAGE_PING) {
        send_status(runtime, &command);
        return true;
    }

    if (runtime->safe_lockout && command.type != CAN_MESSAGE_ENTER_SAFE) {
        send_error_for_command(runtime, &command, CAN_COMMAND_RESULT_BUSY);
        return true;
    }

    can_control_plan_t plan = {0};
    const can_command_result_t plan_result =
        can_control_plan_command(&command, &runtime->state, &plan);
    if (plan_result != CAN_COMMAND_RESULT_OK) {
        increment_invalid_commands(runtime);
        send_error_for_command(runtime, &command, plan_result);
        return true;
    }

    apply_plan(runtime, &plan);
    send_ack(runtime, &command);
    return true;
}
