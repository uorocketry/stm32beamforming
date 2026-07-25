#include "can_protocol.h"

#include <stddef.h>
#include <string.h>

static bool node_field_valid(uint8_t node)
{
    return node <= CAN_NODE_BROADCAST;
}

static bool response_nodes_valid(uint8_t source_node, uint8_t destination_node)
{
    return source_node >= CAN_NODE_MIN
        && source_node <= CAN_NODE_MAX
        && destination_node <= CAN_NODE_MAX;
}

static uint8_t saturate_u16_to_u8(uint16_t value)
{
    return (value > UINT8_MAX) ? UINT8_MAX : (uint8_t)value;
}

static void initialize_frame(can_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->extended = true;
}

bool can_protocol_make_id(
    can_message_type_t type,
    uint8_t destination,
    uint8_t source,
    uint16_t sequence,
    uint32_t *id)
{
    if (id == NULL
        || type >= CAN_MESSAGE_TYPE_COUNT
        || !node_field_valid(destination)
        || !node_field_valid(source)) {
        return false;
    }

    *id = ((uint32_t)type << CAN_ID_TYPE_SHIFT)
        | ((uint32_t)destination << CAN_ID_DESTINATION_SHIFT)
        | ((uint32_t)source << CAN_ID_SOURCE_SHIFT)
        | sequence;
    return true;
}

bool can_protocol_parse_id(
    uint32_t id,
    can_message_type_t *type,
    uint8_t *destination,
    uint8_t *source,
    uint16_t *sequence)
{
    if (id > CAN_EXTENDED_ID_MAX
        || type == NULL
        || destination == NULL
        || source == NULL
        || sequence == NULL) {
        return false;
    }

    *type = (can_message_type_t)((id >> CAN_ID_TYPE_SHIFT) & CAN_ID_TYPE_MASK);
    *destination = (uint8_t)((id >> CAN_ID_DESTINATION_SHIFT) & CAN_ID_NODE_MASK);
    *source = (uint8_t)((id >> CAN_ID_SOURCE_SHIFT) & CAN_ID_NODE_MASK);
    *sequence = (uint16_t)(id & CAN_ID_SEQUENCE_MASK);
    return true;
}

static can_decode_result_t validate_command_payload(
    const can_frame_t *frame,
    can_command_t *command)
{
    switch (command->type) {
    case CAN_MESSAGE_SET_PHASE:
        if (frame->length != 2u) {
            return CAN_DECODE_INVALID_LENGTH;
        }
        if (frame->data[1] > CAN_PHASE_ADDRESS_MAX) {
            return CAN_DECODE_INVALID_PAYLOAD;
        }
        command->phase_state = frame->data[0];
        command->phase_address = frame->data[1];
        return CAN_DECODE_OK;

    case CAN_MESSAGE_SET_VGA:
        if (frame->length != 1u) {
            return CAN_DECODE_INVALID_LENGTH;
        }
        if (frame->data[0] > CAN_VGA_ATTENUATION_MAX_DB) {
            return CAN_DECODE_INVALID_PAYLOAD;
        }
        command->attenuation_db = frame->data[0];
        return CAN_DECODE_OK;

    case CAN_MESSAGE_SET_COMBINED:
        if (frame->length != 3u) {
            return CAN_DECODE_INVALID_LENGTH;
        }
        if (frame->data[1] > CAN_PHASE_ADDRESS_MAX
            || frame->data[2] > CAN_VGA_ATTENUATION_MAX_DB) {
            return CAN_DECODE_INVALID_PAYLOAD;
        }
        command->phase_state = frame->data[0];
        command->phase_address = frame->data[1];
        command->attenuation_db = frame->data[2];
        return CAN_DECODE_OK;

    case CAN_MESSAGE_ENTER_SAFE:
        if (frame->length != 1u) {
            return CAN_DECODE_INVALID_LENGTH;
        }
        if (frame->data[0] > CAN_PHASE_ADDRESS_MAX) {
            return CAN_DECODE_INVALID_PAYLOAD;
        }
        command->phase_address = frame->data[0];
        return CAN_DECODE_OK;

    case CAN_MESSAGE_PING:
        return (frame->length == 0u)
            ? CAN_DECODE_OK
            : CAN_DECODE_INVALID_LENGTH;

    case CAN_MESSAGE_STATUS:
    case CAN_MESSAGE_ACK:
    case CAN_MESSAGE_ERROR:
    case CAN_MESSAGE_TYPE_COUNT:
        return CAN_DECODE_UNSUPPORTED_TYPE;
    }

    return CAN_DECODE_UNSUPPORTED_TYPE;
}

can_decode_result_t can_protocol_decode_command(
    const can_frame_t *frame,
    uint8_t self_node,
    can_command_t *command)
{
    if (frame == NULL
        || command == NULL
        || self_node < CAN_NODE_MIN
        || self_node > CAN_NODE_MAX) {
        return CAN_DECODE_INVALID_ARGUMENT;
    }

    if (!frame->extended || frame->remote) {
        return CAN_DECODE_IGNORED;
    }
    if (frame->length > CAN_MAX_DATA_LENGTH) {
        return CAN_DECODE_INVALID_LENGTH;
    }

    can_command_t decoded = {0};
    if (!can_protocol_parse_id(
            frame->id,
            &decoded.type,
            &decoded.destination,
            &decoded.source,
            &decoded.sequence)) {
        return CAN_DECODE_INVALID_ID;
    }

    if (decoded.destination != self_node
        && decoded.destination != CAN_NODE_BROADCAST) {
        return CAN_DECODE_IGNORED;
    }
    if (decoded.source != CAN_NODE_CONTROLLER) {
        return CAN_DECODE_INVALID_ID;
    }

    const can_decode_result_t result = validate_command_payload(frame, &decoded);
    if (result != CAN_DECODE_OK) {
        return result;
    }

    *command = decoded;
    return CAN_DECODE_OK;
}

static bool make_result_frame(
    can_message_type_t response_type,
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    can_message_type_t command_type,
    can_command_result_t result,
    can_frame_t *frame)
{
    if (frame == NULL
        || !response_nodes_valid(source_node, destination_node)
        || command_type > CAN_MESSAGE_PING
        || result > CAN_COMMAND_RESULT_BUSY) {
        return false;
    }

    initialize_frame(frame);
    if (!can_protocol_make_id(
            response_type,
            destination_node,
            source_node,
            sequence,
            &frame->id)) {
        return false;
    }

    frame->length = 2u;
    frame->data[0] = (uint8_t)command_type;
    frame->data[1] = (uint8_t)result;
    return true;
}

bool can_protocol_make_ack(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    can_message_type_t command_type,
    can_frame_t *frame)
{
    return make_result_frame(
        CAN_MESSAGE_ACK,
        source_node,
        destination_node,
        sequence,
        command_type,
        CAN_COMMAND_RESULT_OK,
        frame);
}

bool can_protocol_make_error(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    can_message_type_t command_type,
    can_command_result_t result,
    can_frame_t *frame)
{
    if (result == CAN_COMMAND_RESULT_OK) {
        return false;
    }

    return make_result_frame(
        CAN_MESSAGE_ERROR,
        source_node,
        destination_node,
        sequence,
        command_type,
        result,
        frame);
}

bool can_protocol_make_status(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    const can_status_payload_t *status,
    can_frame_t *frame)
{
    if (status == NULL
        || frame == NULL
        || status->node_id != source_node
        || !response_nodes_valid(source_node, destination_node)
        || status->phase_address > CAN_PHASE_ADDRESS_MAX
        || status->attenuation_db > CAN_VGA_ATTENUATION_MAX_DB) {
        return false;
    }

    initialize_frame(frame);
    if (!can_protocol_make_id(
            CAN_MESSAGE_STATUS,
            destination_node,
            source_node,
            sequence,
            &frame->id)) {
        return false;
    }

    frame->length = CAN_MAX_DATA_LENGTH;
    frame->data[0] = CAN_PROTOCOL_VERSION;
    frame->data[1] = status->node_id;
    frame->data[2] = status->phase_state;
    frame->data[3] = status->phase_address;
    frame->data[4] = status->attenuation_db;
    frame->data[5] = status->health_flags;
    frame->data[6] = saturate_u16_to_u8(status->rx_dropped);
    frame->data[7] = saturate_u16_to_u8(status->invalid_commands);
    return true;
}

can_command_result_t can_protocol_result_from_decode(can_decode_result_t result)
{
    switch (result) {
    case CAN_DECODE_INVALID_LENGTH:
        return CAN_COMMAND_RESULT_INVALID_LENGTH;
    case CAN_DECODE_INVALID_PAYLOAD:
    case CAN_DECODE_INVALID_ID:
    case CAN_DECODE_INVALID_ARGUMENT:
        return CAN_COMMAND_RESULT_INVALID_PAYLOAD;
    case CAN_DECODE_UNSUPPORTED_TYPE:
        return CAN_COMMAND_RESULT_UNSUPPORTED;
    case CAN_DECODE_OK:
    case CAN_DECODE_IGNORED:
        return CAN_COMMAND_RESULT_OK;
    }

    return CAN_COMMAND_RESULT_INVALID_PAYLOAD;
}
