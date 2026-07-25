import importlib.util
import re
import struct
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "can_smoke_test.py"
HEADER_PATH = Path(__file__).resolve().parents[1] / "app" / "include" / "can_protocol.h"
SPEC = importlib.util.spec_from_file_location("can_smoke_test", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CanSmokeToolTests(unittest.TestCase):
    def test_python_constants_match_c_protocol_header(self) -> None:
        header = HEADER_PATH.read_text(encoding="utf-8")

        def macro(name: str) -> int:
            match = re.search(rf"^#define {name} (0x[0-9a-fA-F]+|[0-9]+)u$", header, re.MULTILINE)
            self.assertIsNotNone(match, name)
            assert match is not None
            return int(match.group(1), 0)

        def enum_value(name: str) -> int:
            match = re.search(rf"^    {name} = ([0-9]+),?$", header, re.MULTILINE)
            self.assertIsNotNone(match, name)
            assert match is not None
            return int(match.group(1), 10)

        self.assertEqual(MODULE.CAN_PROTOCOL_VERSION, macro("CAN_PROTOCOL_VERSION"))
        self.assertEqual(MODULE.CAN_NODE_CONTROLLER, macro("CAN_NODE_CONTROLLER"))
        self.assertEqual(MODULE.CAN_NODE_MIN, macro("CAN_NODE_MIN"))
        self.assertEqual(MODULE.CAN_NODE_MAX, macro("CAN_NODE_MAX"))
        self.assertEqual(MODULE.CAN_NODE_BROADCAST, macro("CAN_NODE_BROADCAST"))
        self.assertEqual(MODULE.CAN_ID_TYPE_SHIFT, macro("CAN_ID_TYPE_SHIFT"))
        self.assertEqual(
            MODULE.CAN_ID_DESTINATION_SHIFT,
            macro("CAN_ID_DESTINATION_SHIFT"),
        )
        self.assertEqual(MODULE.CAN_ID_SOURCE_SHIFT, macro("CAN_ID_SOURCE_SHIFT"))

        for message_type in MODULE.MessageType:
            self.assertEqual(
                int(message_type),
                enum_value(f"CAN_MESSAGE_{message_type.name}"),
            )

    def test_identifier_layout_matches_firmware(self) -> None:
        identifier = MODULE.make_identifier(
            MODULE.MessageType.SET_PHASE,
            destination=1,
            source=0,
            sequence=0x1234,
        )
        self.assertEqual(identifier, 0x08201234)

    def test_linux_frame_round_trip(self) -> None:
        raw = MODULE.pack_linux_can_frame(0x08201234, bytes([146, 3]))
        self.assertEqual(len(raw), 16)

        can_id, length = struct.unpack_from("=IB", raw)
        self.assertEqual(can_id, MODULE.CAN_EFF_FLAG | 0x08201234)
        self.assertEqual(length, 2)

        decoded = MODULE.unpack_linux_can_frame(raw)
        self.assertTrue(decoded.extended)
        self.assertFalse(decoded.remote)
        self.assertEqual(decoded.identifier, 0x08201234)
        self.assertEqual(decoded.data, bytes([146, 3]))

    def test_command_payload_validation(self) -> None:
        self.assertEqual(
            MODULE.command_payload(MODULE.MessageType.SET_VGA, [23]),
            bytes([23]),
        )
        with self.assertRaises(ValueError):
            MODULE.command_payload(MODULE.MessageType.SET_VGA, [24])
        with self.assertRaises(ValueError):
            MODULE.make_identifier(MODULE.MessageType.PING, 32, 0, 0)


if __name__ == "__main__":
    unittest.main()
