#!/usr/bin/env python3
"""Send protocol-v1 smoke-test commands over Linux SocketCAN."""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from enum import IntEnum
from typing import NamedTuple, Sequence

CAN_EFF_FLAG = 0x80000000
CAN_RTR_FLAG = 0x40000000
CAN_ERR_FLAG = 0x20000000
CAN_EFF_MASK = 0x1FFFFFFF
CAN_FRAME_STRUCT = struct.Struct("=IB3x8s")

CAN_NODE_CONTROLLER = 0
CAN_NODE_MIN = 1
CAN_NODE_MAX = 30
CAN_NODE_BROADCAST = 31
CAN_PROTOCOL_VERSION = 1

CAN_ID_TYPE_SHIFT = 26
CAN_ID_DESTINATION_SHIFT = 21
CAN_ID_SOURCE_SHIFT = 16
CAN_ID_TYPE_MASK = 0x07
CAN_ID_NODE_MASK = 0x1F
CAN_ID_SEQUENCE_MASK = 0xFFFF


class MessageType(IntEnum):
    ENTER_SAFE = 0
    SET_COMBINED = 1
    SET_PHASE = 2
    SET_VGA = 3
    PING = 4
    STATUS = 5
    ACK = 6
    ERROR = 7


class CommandResult(IntEnum):
    OK = 0
    INVALID_LENGTH = 1
    INVALID_PAYLOAD = 2
    UNSUPPORTED = 3
    HARDWARE = 4
    BUSY = 5


class DecodedFrame(NamedTuple):
    identifier: int
    extended: bool
    remote: bool
    error: bool
    data: bytes


class IdentifierFields(NamedTuple):
    message_type: MessageType
    destination: int
    source: int
    sequence: int


def make_identifier(
    message_type: MessageType,
    destination: int,
    source: int,
    sequence: int,
) -> int:
    if not isinstance(message_type, MessageType):
        raise ValueError("message_type must be a MessageType")
    if not 0 <= destination <= CAN_NODE_BROADCAST:
        raise ValueError("destination must be in the range 0..31")
    if not 0 <= source <= CAN_NODE_BROADCAST:
        raise ValueError("source must be in the range 0..31")
    if not 0 <= sequence <= CAN_ID_SEQUENCE_MASK:
        raise ValueError("sequence must be in the range 0..65535")

    return (
        (int(message_type) << CAN_ID_TYPE_SHIFT)
        | (destination << CAN_ID_DESTINATION_SHIFT)
        | (source << CAN_ID_SOURCE_SHIFT)
        | sequence
    )


def parse_identifier(identifier: int) -> IdentifierFields:
    if not 0 <= identifier <= CAN_EFF_MASK:
        raise ValueError("identifier exceeds the 29-bit CAN range")

    return IdentifierFields(
        MessageType((identifier >> CAN_ID_TYPE_SHIFT) & CAN_ID_TYPE_MASK),
        (identifier >> CAN_ID_DESTINATION_SHIFT) & CAN_ID_NODE_MASK,
        (identifier >> CAN_ID_SOURCE_SHIFT) & CAN_ID_NODE_MASK,
        identifier & CAN_ID_SEQUENCE_MASK,
    )


def command_payload(message_type: MessageType, values: Sequence[int]) -> bytes:
    values = list(values)

    if message_type is MessageType.SET_PHASE:
        if len(values) != 2 or not 0 <= values[0] <= 255 or not 0 <= values[1] <= 15:
            raise ValueError("phase requires STATE 0..255 and ADDRESS 0..15")
    elif message_type is MessageType.SET_VGA:
        if len(values) != 1 or not 0 <= values[0] <= 23:
            raise ValueError("vga requires ATTENUATION_DB 0..23")
    elif message_type is MessageType.SET_COMBINED:
        if (
            len(values) != 3
            or not 0 <= values[0] <= 255
            or not 0 <= values[1] <= 15
            or not 0 <= values[2] <= 23
        ):
            raise ValueError(
                "combined requires STATE 0..255, ADDRESS 0..15, and ATTENUATION_DB 0..23"
            )
    elif message_type is MessageType.ENTER_SAFE:
        if len(values) != 1 or not 0 <= values[0] <= 15:
            raise ValueError("safe requires ADDRESS 0..15")
    elif message_type is MessageType.PING:
        if values:
            raise ValueError("ping has no payload")
    else:
        raise ValueError("only command message types can be transmitted")

    return bytes(values)


def pack_linux_can_frame(identifier: int, data: bytes) -> bytes:
    if not 0 <= identifier <= CAN_EFF_MASK:
        raise ValueError("identifier exceeds the 29-bit CAN range")
    if len(data) > 8:
        raise ValueError("classic CAN payloads are limited to 8 bytes")

    return CAN_FRAME_STRUCT.pack(
        identifier | CAN_EFF_FLAG,
        len(data),
        data.ljust(8, b"\x00"),
    )


def unpack_linux_can_frame(raw: bytes) -> DecodedFrame:
    if len(raw) != CAN_FRAME_STRUCT.size:
        raise ValueError(f"expected {CAN_FRAME_STRUCT.size} bytes, received {len(raw)}")

    can_id, length, data = CAN_FRAME_STRUCT.unpack(raw)
    if length > 8:
        raise ValueError(f"invalid classic CAN length {length}")

    return DecodedFrame(
        identifier=can_id & CAN_EFF_MASK,
        extended=(can_id & CAN_EFF_FLAG) != 0,
        remote=(can_id & CAN_RTR_FLAG) != 0,
        error=(can_id & CAN_ERR_FLAG) != 0,
        data=data[:length],
    )


def receive_matching_response(
    sock: socket.socket,
    expected_source: int,
    sequence: int,
    timeout: float,
) -> tuple[IdentifierFields, bytes]:
    deadline = time.monotonic() + timeout

    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("timed out waiting for a matching CAN response")
        sock.settimeout(remaining)

        frame = unpack_linux_can_frame(sock.recv(CAN_FRAME_STRUCT.size))
        if not frame.extended or frame.remote or frame.error:
            continue

        fields = parse_identifier(frame.identifier)
        if (
            fields.destination == CAN_NODE_CONTROLLER
            and fields.source == expected_source
            and fields.sequence == sequence
            and fields.message_type in (MessageType.STATUS, MessageType.ACK, MessageType.ERROR)
        ):
            return fields, frame.data


def print_response(fields: IdentifierFields, data: bytes) -> int:
    if fields.message_type in (MessageType.ACK, MessageType.ERROR):
        if len(data) != 2:
            raise ValueError("ACK/ERROR response must contain two bytes")
        command_type = MessageType(data[0])
        result = CommandResult(data[1])
        print(
            f"{fields.message_type.name}: command={command_type.name} "
            f"result={result.name} sequence={fields.sequence}"
        )
        return 0 if fields.message_type is MessageType.ACK else 2

    if len(data) != 8:
        raise ValueError("STATUS response must contain eight bytes")
    if data[0] != CAN_PROTOCOL_VERSION:
        raise ValueError(f"unsupported status protocol version {data[0]}")

    print(
        "STATUS: "
        f"node={data[1]} phase_state={data[2]} phase_address={data[3]} "
        f"attenuation_db={data[4]} health_flags=0x{data[5]:02x} "
        f"rx_dropped={data[6]} invalid_commands={data[7]} "
        f"sequence={fields.sequence}"
    )
    return 0


def transact(
    interface: str,
    node: int,
    message_type: MessageType,
    values: Sequence[int],
    sequence: int,
    timeout: float,
) -> int:
    if not CAN_NODE_MIN <= node <= CAN_NODE_MAX:
        raise ValueError("node must be in the range 1..30")
    if not hasattr(socket, "PF_CAN") or not hasattr(socket, "CAN_RAW"):
        raise RuntimeError("SocketCAN is available only on supported Linux systems")

    identifier = make_identifier(
        message_type,
        destination=node,
        source=CAN_NODE_CONTROLLER,
        sequence=sequence,
    )
    payload = command_payload(message_type, values)

    with socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW) as sock:
        sock.bind((interface,))
        sock.send(pack_linux_can_frame(identifier, payload))
        fields, response_data = receive_matching_response(sock, node, sequence, timeout)

    return print_response(fields, response_data)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--interface", default="can0", help="SocketCAN interface")
    result.add_argument("--sequence", type=lambda value: int(value, 0))
    result.add_argument("--timeout", type=float, default=1.0)

    subparsers = result.add_subparsers(dest="action", required=True)

    ping = subparsers.add_parser("ping", help="request node status")
    ping.add_argument("node", type=int)

    phase = subparsers.add_parser("phase", help="set one phase-shifter state")
    phase.add_argument("node", type=int)
    phase.add_argument("state", type=int)
    phase.add_argument("address", type=int)

    vga = subparsers.add_parser("vga", help="set VGA attenuation")
    vga.add_argument("node", type=int)
    vga.add_argument("attenuation_db", type=int)

    combined = subparsers.add_parser("combined", help="safely set phase and attenuation")
    combined.add_argument("node", type=int)
    combined.add_argument("state", type=int)
    combined.add_argument("address", type=int)
    combined.add_argument("attenuation_db", type=int)

    safe = subparsers.add_parser("safe", help="set maximum attenuation and zero phase")
    safe.add_argument("node", type=int)
    safe.add_argument("address", type=int)

    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    sequence = (
        args.sequence
        if args.sequence is not None
        else (time.monotonic_ns() >> 10) & CAN_ID_SEQUENCE_MASK
    )

    action_map = {
        "ping": (MessageType.PING, []),
        "phase": (MessageType.SET_PHASE, [getattr(args, "state", 0), getattr(args, "address", 0)]),
        "vga": (MessageType.SET_VGA, [getattr(args, "attenuation_db", 0)]),
        "combined": (
            MessageType.SET_COMBINED,
            [
                getattr(args, "state", 0),
                getattr(args, "address", 0),
                getattr(args, "attenuation_db", 0),
            ],
        ),
        "safe": (MessageType.ENTER_SAFE, [getattr(args, "address", 0)]),
    }
    message_type, values = action_map[args.action]

    try:
        return transact(
            args.interface,
            args.node,
            message_type,
            values,
            sequence,
            args.timeout,
        )
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
