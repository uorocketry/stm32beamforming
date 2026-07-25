#include "can_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static can_frame_t frame_for(
    can_message_type_t type,
    uint8_t destination,
    uint8_t source,
    uint16_t sequence,
    const uint8_t *data,
    uint8_t length)
{
    can_frame_t frame = {0};
    assert(can_protocol_make_id(type, destination, source, sequence, &frame.id));
    frame.extended = true;
    frame.remote = false;
    frame.length = length;
    if (data != NULL && length > 0u) {
        memcpy(frame.data, data, length);
    }
    return frame;
}

static void test_identifier_round_trip(void)
{
    uint32_t id = 0u;
    assert(can_protocol_make_id(
        CAN_MESSAGE_SET_PHASE,
        1u,
        CAN_NODE_CONTROLLER,
        0x1234u,
        &id));
    assert(id == 0x08201234u);

    can_message_type_t type = CAN_MESSAGE_ERROR;
    uint8_t destination = 0u;
    uint8_t source = 0u;
    uint16_t sequence = 0u;
    assert(can_protocol_parse_id(id, &type, &destination, &source, &sequence));
    assert(type == CAN_MESSAGE_SET_PHASE);
    assert(destination == 1u);
    assert(source == CAN_NODE_CONTROLLER);
    assert(sequence == 0x1234u);
}

static void test_identifier_rejects_invalid_fields(void)
{
    uint32_t id = 0u;
    assert(!can_protocol_make_id(
        (can_message_type_t)CAN_MESSAGE_TYPE_COUNT,
        1u,
        0u,
        0u,
        &id));
    assert(!can_protocol_make_id(CAN_MESSAGE_PING, 32u, 0u, 0u, &id));
    assert(!can_protocol_make_id(CAN_MESSAGE_PING, 1u, 32u, 0u, &id));
    assert(!can_protocol_make_id(CAN_MESSAGE_PING, 1u, 0u, 0u, NULL));
}

static void test_safe_command_has_highest_command_arbitration_priority(void)
{
    uint32_t safe_id = 0u;
    uint32_t combined_id = 0u;
    uint32_t phase_id = 0u;
    uint32_t vga_id = 0u;

    assert(can_protocol_make_id(CAN_MESSAGE_ENTER_SAFE, 1u, 0u, 1u, &safe_id));
    assert(can_protocol_make_id(CAN_MESSAGE_SET_COMBINED, 1u, 0u, 1u, &combined_id));
    assert(can_protocol_make_id(CAN_MESSAGE_SET_PHASE, 1u, 0u, 1u, &phase_id));
    assert(can_protocol_make_id(CAN_MESSAGE_SET_VGA, 1u, 0u, 1u, &vga_id));

    assert(safe_id < combined_id);
    assert(combined_id < phase_id);
    assert(phase_id < vga_id);
}

static void test_decode_phase_command(void)
{
    const uint8_t payload[] = {146u, 3u};
    const can_frame_t frame = frame_for(
        CAN_MESSAGE_SET_PHASE,
        1u,
        CAN_NODE_CONTROLLER,
        7u,
        payload,
        sizeof(payload));

    can_command_t command = {0};
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_OK);
    assert(command.type == CAN_MESSAGE_SET_PHASE);
    assert(command.source == CAN_NODE_CONTROLLER);
    assert(command.destination == 1u);
    assert(command.sequence == 7u);
    assert(command.phase_state == 146u);
    assert(command.phase_address == 3u);
}

static void test_decode_broadcast_combined_command(void)
{
    const uint8_t payload[] = {64u, 4u, 23u};
    const can_frame_t frame = frame_for(
        CAN_MESSAGE_SET_COMBINED,
        CAN_NODE_BROADCAST,
        CAN_NODE_CONTROLLER,
        8u,
        payload,
        sizeof(payload));

    can_command_t command = {0};
    assert(can_protocol_decode_command(&frame, 5u, &command) == CAN_DECODE_OK);
    assert(command.phase_state == 64u);
    assert(command.phase_address == 4u);
    assert(command.attenuation_db == 23u);
}

static void test_decode_ignores_other_nodes_and_non_commands(void)
{
    can_command_t command = {0};
    can_frame_t frame = frame_for(
        CAN_MESSAGE_PING,
        2u,
        CAN_NODE_CONTROLLER,
        1u,
        NULL,
        0u);
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_IGNORED);

    frame.extended = false;
    frame.id = 0x123u;
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_IGNORED);

    frame = frame_for(
        CAN_MESSAGE_PING,
        1u,
        CAN_NODE_CONTROLLER,
        1u,
        NULL,
        0u);
    frame.remote = true;
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_IGNORED);

    frame = frame_for(
        CAN_MESSAGE_STATUS,
        1u,
        CAN_NODE_CONTROLLER,
        1u,
        NULL,
        0u);
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_UNSUPPORTED_TYPE);
}

static void test_decode_validates_lengths_and_payloads(void)
{
    can_command_t command = {0};
    const uint8_t phase_payload[] = {1u, 16u};
    can_frame_t frame = frame_for(
        CAN_MESSAGE_SET_PHASE,
        1u,
        0u,
        1u,
        phase_payload,
        sizeof(phase_payload));
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_INVALID_PAYLOAD);

    const uint8_t vga_payload[] = {24u};
    frame = frame_for(
        CAN_MESSAGE_SET_VGA,
        1u,
        0u,
        1u,
        vga_payload,
        sizeof(vga_payload));
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_INVALID_PAYLOAD);

    const uint8_t combined_payload[] = {1u, 3u};
    frame = frame_for(
        CAN_MESSAGE_SET_COMBINED,
        1u,
        0u,
        1u,
        combined_payload,
        sizeof(combined_payload));
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_INVALID_LENGTH);

    frame = frame_for(
        CAN_MESSAGE_PING,
        1u,
        0u,
        1u,
        vga_payload,
        1u);
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_INVALID_LENGTH);

    frame = frame_for(
        CAN_MESSAGE_PING,
        1u,
        2u,
        1u,
        NULL,
        0u);
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_INVALID_ID);
}

static void test_safe_and_ping_commands(void)
{
    can_command_t command = {0};
    const uint8_t safe_payload[] = {3u};
    can_frame_t frame = frame_for(
        CAN_MESSAGE_ENTER_SAFE,
        1u,
        0u,
        9u,
        safe_payload,
        sizeof(safe_payload));
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_OK);
    assert(command.phase_address == 3u);

    frame = frame_for(CAN_MESSAGE_PING, 1u, 0u, 10u, NULL, 0u);
    assert(can_protocol_decode_command(&frame, 1u, &command) == CAN_DECODE_OK);
    assert(command.type == CAN_MESSAGE_PING);
}

static void test_response_builders(void)
{
    can_frame_t response = {0};
    assert(can_protocol_make_ack(1u, 0u, 42u, CAN_MESSAGE_SET_PHASE, &response));
    assert(response.extended);
    assert(!response.remote);
    assert(response.length == 2u);
    assert(response.data[0] == CAN_MESSAGE_SET_PHASE);
    assert(response.data[1] == CAN_COMMAND_RESULT_OK);

    can_message_type_t type = CAN_MESSAGE_SET_PHASE;
    uint8_t destination = 0u;
    uint8_t source = 0u;
    uint16_t sequence = 0u;
    assert(can_protocol_parse_id(response.id, &type, &destination, &source, &sequence));
    assert(type == CAN_MESSAGE_ACK);
    assert(destination == 0u);
    assert(source == 1u);
    assert(sequence == 42u);

    assert(can_protocol_make_error(
        1u,
        0u,
        43u,
        CAN_MESSAGE_SET_VGA,
        CAN_COMMAND_RESULT_INVALID_PAYLOAD,
        &response));
    assert(response.length == 2u);
    assert(response.data[0] == CAN_MESSAGE_SET_VGA);
    assert(response.data[1] == CAN_COMMAND_RESULT_INVALID_PAYLOAD);

    const can_status_payload_t status = {
        .node_id = 1u,
        .phase_state = 146u,
        .phase_address = 3u,
        .attenuation_db = 23u,
        .health_flags = CAN_HEALTH_CLOCK_FALLBACK,
        .rx_dropped = 300u,
        .invalid_commands = 2u,
    };
    assert(can_protocol_make_status(1u, 0u, 44u, &status, &response));
    assert(response.length == 8u);
    assert(response.data[0] == CAN_PROTOCOL_VERSION);
    assert(response.data[1] == 1u);
    assert(response.data[2] == 146u);
    assert(response.data[3] == 3u);
    assert(response.data[4] == 23u);
    assert(response.data[5] == CAN_HEALTH_CLOCK_FALLBACK);
    assert(response.data[6] == 255u);
    assert(response.data[7] == 2u);
}

int main(void)
{
    test_identifier_round_trip();
    test_identifier_rejects_invalid_fields();
    test_safe_command_has_highest_command_arbitration_priority();
    test_decode_phase_command();
    test_decode_broadcast_combined_command();
    test_decode_ignores_other_nodes_and_non_commands();
    test_decode_validates_lengths_and_payloads();
    test_safe_and_ping_commands();
    test_response_builders();
    puts("CAN protocol tests passed");
    return 0;
}
