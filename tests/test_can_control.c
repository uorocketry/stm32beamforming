#include "can_control.h"
#include "beamforming_protocol.h"

#include <assert.h>
#include <stdio.h>

static can_command_t command_of(can_message_type_t type)
{
    can_command_t command = {0};
    command.type = type;
    command.source = CAN_NODE_CONTROLLER;
    command.destination = 1u;
    command.sequence = 1u;
    return command;
}

static can_control_state_t initial_state(void)
{
    const can_control_state_t state = {
        .phase_state = 0u,
        .phase_address = 3u,
        .attenuation_db = 23u,
    };
    return state;
}

static void assert_operation(
    const can_control_plan_t *plan,
    uint8_t index,
    can_control_operation_type_t type,
    uint16_t command)
{
    assert(index < plan->operation_count);
    assert(plan->operations[index].type == type);
    assert(plan->operations[index].command == command);
}

static void test_phase_plan(void)
{
    const can_control_state_t current = initial_state();
    can_command_t command = command_of(CAN_MESSAGE_SET_PHASE);
    command.phase_state = 146u;
    command.phase_address = 3u;

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 1u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_PHASE, 0x092cu);
    assert(plan.resulting_state.phase_state == 146u);
    assert(plan.resulting_state.phase_address == 3u);
    assert(plan.resulting_state.attenuation_db == 23u);
}

static void test_phase_plan_temporarily_attenuates_and_restores_active_output(void)
{
    const can_control_state_t current = {
        .phase_state = 10u,
        .phase_address = 3u,
        .attenuation_db = 8u,
    };
    can_command_t command = command_of(CAN_MESSAGE_SET_PHASE);
    command.phase_state = 146u;
    command.phase_address = 3u;

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 3u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_VGA, 0x5cu);
    assert_operation(&plan, 1u, CAN_CONTROL_OPERATION_PHASE, 0x092cu);
    assert_operation(&plan, 2u, CAN_CONTROL_OPERATION_VGA, 0x20u);
    assert(plan.resulting_state.phase_state == 146u);
    assert(plan.resulting_state.attenuation_db == 8u);
}

static void test_vga_plan(void)
{
    const can_control_state_t current = initial_state();
    can_command_t command = command_of(CAN_MESSAGE_SET_VGA);
    command.attenuation_db = 8u;

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 1u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_VGA, 0x20u);
    assert(plan.resulting_state.attenuation_db == 8u);
}

static void test_combined_plan_uses_safe_transition_order(void)
{
    const can_control_state_t current = initial_state();
    can_command_t command = command_of(CAN_MESSAGE_SET_COMBINED);
    command.phase_state = 64u;
    command.phase_address = 4u;
    command.attenuation_db = 12u;

    uint16_t expected_phase = 0u;
    assert(phase_command_from_state(64u, 4u, &expected_phase));

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 3u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_VGA, 0x5cu);
    assert_operation(&plan, 1u, CAN_CONTROL_OPERATION_PHASE, expected_phase);
    assert_operation(&plan, 2u, CAN_CONTROL_OPERATION_VGA, 0x30u);
    assert(plan.resulting_state.phase_state == 64u);
    assert(plan.resulting_state.phase_address == 4u);
    assert(plan.resulting_state.attenuation_db == 12u);
}

static void test_combined_plan_avoids_duplicate_max_attenuation_write(void)
{
    const can_control_state_t current = initial_state();
    can_command_t command = command_of(CAN_MESSAGE_SET_COMBINED);
    command.phase_state = 64u;
    command.phase_address = 4u;
    command.attenuation_db = 23u;

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 2u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_VGA, 0x5cu);
    assert(plan.operations[1].type == CAN_CONTROL_OPERATION_PHASE);
}

static void test_safe_plan_forces_maximum_attenuation_and_zero_phase(void)
{
    const can_control_state_t current = {
        .phase_state = 200u,
        .phase_address = 2u,
        .attenuation_db = 0u,
    };
    can_command_t command = command_of(CAN_MESSAGE_ENTER_SAFE);
    command.phase_address = 3u;

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 2u);
    assert_operation(&plan, 0u, CAN_CONTROL_OPERATION_VGA, 0x5cu);
    assert_operation(&plan, 1u, CAN_CONTROL_OPERATION_PHASE, 0x000cu);
    assert(plan.resulting_state.phase_state == 0u);
    assert(plan.resulting_state.phase_address == 3u);
    assert(plan.resulting_state.attenuation_db == 23u);
}

static void test_ping_has_no_hardware_actions(void)
{
    const can_control_state_t current = {
        .phase_state = 1u,
        .phase_address = 2u,
        .attenuation_db = 3u,
    };
    const can_command_t command = command_of(CAN_MESSAGE_PING);

    can_control_plan_t plan = {0};
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_OK);
    assert(plan.operation_count == 0u);
    assert(plan.resulting_state.phase_state == current.phase_state);
    assert(plan.resulting_state.attenuation_db == current.attenuation_db);
}

static void test_invalid_arguments_and_unsupported_types(void)
{
    const can_control_state_t current = {0};
    can_control_plan_t plan = {0};
    can_command_t command = command_of(CAN_MESSAGE_STATUS);

    assert(can_control_plan_command(NULL, &current, &plan) == CAN_COMMAND_RESULT_INVALID_PAYLOAD);
    assert(can_control_plan_command(&command, NULL, &plan) == CAN_COMMAND_RESULT_INVALID_PAYLOAD);
    assert(can_control_plan_command(&command, &current, NULL) == CAN_COMMAND_RESULT_INVALID_PAYLOAD);
    assert(can_control_plan_command(&command, &current, &plan) == CAN_COMMAND_RESULT_UNSUPPORTED);
}

int main(void)
{
    test_phase_plan();
    test_phase_plan_temporarily_attenuates_and_restores_active_output();
    test_vga_plan();
    test_combined_plan_uses_safe_transition_order();
    test_combined_plan_avoids_duplicate_max_attenuation_write();
    test_safe_plan_forces_maximum_attenuation_and_zero_phase();
    test_ping_has_no_hardware_actions();
    test_invalid_arguments_and_unsupported_types();
    puts("CAN control tests passed");
    return 0;
}
