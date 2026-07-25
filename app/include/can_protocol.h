#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_PROTOCOL_VERSION 1u
#define CAN_EXTENDED_ID_MAX 0x1fffffffu
#define CAN_NODE_CONTROLLER 0u
#define CAN_NODE_MIN 1u
#define CAN_NODE_MAX 30u
#define CAN_NODE_BROADCAST 31u
#define CAN_MAX_DATA_LENGTH 8u

#define CAN_ID_TYPE_SHIFT 26u
#define CAN_ID_DESTINATION_SHIFT 21u
#define CAN_ID_SOURCE_SHIFT 16u
#define CAN_ID_TYPE_MASK 0x07u
#define CAN_ID_NODE_MASK 0x1fu
#define CAN_ID_SEQUENCE_MASK 0xffffu

#define CAN_PHASE_ADDRESS_MAX 15u
#define CAN_VGA_ATTENUATION_MAX_DB 23u

typedef enum can_message_type {
    CAN_MESSAGE_ENTER_SAFE = 0,
    CAN_MESSAGE_SET_COMBINED = 1,
    CAN_MESSAGE_SET_PHASE = 2,
    CAN_MESSAGE_SET_VGA = 3,
    CAN_MESSAGE_PING = 4,
    CAN_MESSAGE_STATUS = 5,
    CAN_MESSAGE_ACK = 6,
    CAN_MESSAGE_ERROR = 7,
    CAN_MESSAGE_TYPE_COUNT = 8
} can_message_type_t;

typedef enum can_decode_result {
    CAN_DECODE_OK = 0,
    CAN_DECODE_IGNORED = 1,
    CAN_DECODE_INVALID_ARGUMENT = 2,
    CAN_DECODE_INVALID_ID = 3,
    CAN_DECODE_INVALID_LENGTH = 4,
    CAN_DECODE_INVALID_PAYLOAD = 5,
    CAN_DECODE_UNSUPPORTED_TYPE = 6
} can_decode_result_t;

typedef enum can_command_result {
    CAN_COMMAND_RESULT_OK = 0,
    CAN_COMMAND_RESULT_INVALID_LENGTH = 1,
    CAN_COMMAND_RESULT_INVALID_PAYLOAD = 2,
    CAN_COMMAND_RESULT_UNSUPPORTED = 3,
    CAN_COMMAND_RESULT_HARDWARE = 4,
    CAN_COMMAND_RESULT_BUSY = 5
} can_command_result_t;

typedef enum can_health_flag {
    CAN_HEALTH_NONE = 0,
    CAN_HEALTH_CLOCK_FALLBACK = 1u << 0,
    CAN_HEALTH_RX_DROPPED = 1u << 1,
    CAN_HEALTH_INVALID_COMMAND = 1u << 2,
    CAN_HEALTH_SAFE_LOCKOUT = 1u << 3,
    CAN_HEALTH_BUS_OFF = 1u << 4,
    CAN_HEALTH_TX_DROPPED = 1u << 5
} can_health_flag_t;

typedef struct can_frame {
    uint32_t id;
    bool extended;
    bool remote;
    uint8_t length;
    uint8_t data[CAN_MAX_DATA_LENGTH];
} can_frame_t;

typedef struct can_command {
    can_message_type_t type;
    uint8_t destination;
    uint8_t source;
    uint16_t sequence;
    uint8_t phase_state;
    uint8_t phase_address;
    uint8_t attenuation_db;
} can_command_t;

typedef struct can_status_payload {
    uint8_t node_id;
    uint8_t phase_state;
    uint8_t phase_address;
    uint8_t attenuation_db;
    uint8_t health_flags;
    uint16_t rx_dropped;
    uint16_t invalid_commands;
} can_status_payload_t;

bool can_protocol_make_id(
    can_message_type_t type,
    uint8_t destination,
    uint8_t source,
    uint16_t sequence,
    uint32_t *id);
bool can_protocol_parse_id(
    uint32_t id,
    can_message_type_t *type,
    uint8_t *destination,
    uint8_t *source,
    uint16_t *sequence);
can_decode_result_t can_protocol_decode_command(
    const can_frame_t *frame,
    uint8_t self_node,
    can_command_t *command);
bool can_protocol_make_ack(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    can_message_type_t command_type,
    can_frame_t *frame);
bool can_protocol_make_error(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    can_message_type_t command_type,
    can_command_result_t result,
    can_frame_t *frame);
bool can_protocol_make_status(
    uint8_t source_node,
    uint8_t destination_node,
    uint16_t sequence,
    const can_status_payload_t *status,
    can_frame_t *frame);
can_command_result_t can_protocol_result_from_decode(can_decode_result_t result);

#endif
