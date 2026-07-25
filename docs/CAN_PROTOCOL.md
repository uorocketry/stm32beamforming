# Receiver-board CAN protocol

This document defines protocol version 1 for communication between one controller and multiple STM32F072 receiver-chain boards.

The controller algorithm belongs in a separate host-side project. This repository implements only the per-board firmware, device drivers, command validation, safe output sequencing, and CAN transport.

## Physical and link layer

- Classic CAN 2.0B extended frames
- 500 kbit/s
- Eight data bytes maximum
- Controller node ID: `0`
- Receiver node IDs: `1` through `30`
- Broadcast destination: `31`
- Remote frames and standard-ID frames are ignored
- Each receiver configures hardware acceptance filters for its own destination and broadcast

The current board code uses CAN1 on PA11/PA12 with an external SN65HVD230-class 3.3 V transceiver. Connector choice does not affect the protocol. Verify the actual RJ11 or RJ45 pinout from the board schematic before connecting hardware.

## Extended identifier

The 29-bit identifier is partitioned as follows:

| Bits | Width | Field |
|---:|---:|---|
| 28:26 | 3 | Message type |
| 25:21 | 5 | Destination node |
| 20:16 | 5 | Source node |
| 15:0 | 16 | Sequence number |

Equivalent construction:

```text
id = (type << 26) | (destination << 21) | (source << 16) | sequence
```

Receiver boards accept control commands only from controller node `0`. A receiver never treats another receiver as a valid command source.

The sequence number is echoed in responses. The controller should increment it for each logical request and match responses by source and sequence.

Sequence numbers provide request/response correlation only. Commands are absolute and idempotent, so a controller may safely retry after a timeout, but receivers do not currently maintain a replay cache.

## Message types

| Type | Name | Direction | Payload |
|---:|---|---|---|
| 0 | `ENTER_SAFE` | Controller → receiver | `[phase_address]` |
| 1 | `SET_COMBINED` | Controller → receiver | `[phase_state, phase_address, attenuation_db]` |
| 2 | `SET_PHASE` | Controller → receiver | `[phase_state, phase_address]` |
| 3 | `SET_VGA` | Controller → receiver | `[attenuation_db]` |
| 4 | `PING` | Controller → receiver | Empty |
| 5 | `STATUS` | Receiver → controller | Eight-byte status payload |
| 6 | `ACK` | Receiver → controller | `[command_type, result]` |
| 7 | `ERROR` | Receiver → controller | `[command_type, result]` |

### Field ranges

- `phase_state`: `0..255`
- `phase_address`: `0..15`
- `attenuation_db`: `0..23`

The PE44820 OPT bit is not sent over CAN. Firmware derives it from phase-state bit D6.

The safety command has the numerically lowest type field, so it wins CAN arbitration against every other protocol message type.

Protocol version 1 changes one phase-shifter address per command. A controller can sequence commands for multiple addresses, but an atomic multi-address batch is intentionally not defined until the board channel count and update requirements are confirmed.

## Command behavior

### SET_PHASE

Programs one phase state and address. When the current attenuation is below 23 dB, firmware first applies 23 dB, changes phase, and then restores the prior attenuation. When the VGA is already at 23 dB, only the phase write is needed.

### SET_VGA

Programs one attenuation value from 0 through 23 dB. The current phase setting is left unchanged.

### SET_COMBINED

Uses a safe three-step transition when the final attenuation is below 23 dB:

1. Apply 23 dB maximum attenuation.
2. Program the phase shifter.
3. Apply the requested attenuation.

When the requested final attenuation is already 23 dB, the duplicate final VGA write is omitted.

### ENTER_SAFE

Applies 23 dB attenuation, then phase state zero at the supplied phase address.

### PING

Does not touch hardware. A unicast ping returns `STATUS`.

## Responses

Unicast state-changing commands receive `ACK` after all requested hardware operations complete successfully. Invalid unicast commands receive `ERROR` when the identifier can be safely associated with controller node `0`.

Broadcast commands do not generate responses. This avoids an acknowledgement storm when multiple receiver boards share the bus.

### Result codes

| Value | Meaning |
|---:|---|
| 0 | OK |
| 1 | Invalid payload length |
| 2 | Invalid field value or identifier |
| 3 | Unsupported message type |
| 4 | Hardware operation failed |
| 5 | Receiver busy or retained safe lockout active |

Runtime SPI failures are treated as firmware faults and reset the MCU rather than continuing with an uncertain RF state. Consequently, the controller may observe a missing acknowledgement instead of result code 4.

### STATUS payload

| Byte | Field |
|---:|---|
| 0 | Protocol version |
| 1 | Receiver node ID |
| 2 | Current phase state |
| 3 | Current phase address |
| 4 | Current attenuation in dB |
| 5 | Health flags |
| 6 | Saturated receive-drop count |
| 7 | Saturated invalid-command count |

Health flags:

| Bit | Meaning |
|---:|---|
| 0 | Running on HSI48 fallback instead of HSE/PLL |
| 1 | One or more CAN receive frames were dropped |
| 2 | One or more invalid commands were observed |
| 3 | Retained safe lockout is active |
| 4 | bxCAN currently reports bus-off |
| 5 | One or more response frames could not be queued into a transmit mailbox |

The receive-overrun count is best-effort. bxCAN has a three-frame hardware FIFO, and an overrun arriving during FIFO release can be difficult to observe reliably. The software ISR drains the hardware FIFO into an eight-frame single-producer/single-consumer queue as quickly as possible.

## Building node-specific firmware

The node ID is compiled into the image:

```bash
make -C libopencm3 TARGETS=stm32/f0
make -C app clean all CAN_NODE_ID=1
```

Valid values are `1..30`. An invalid value fails compilation.

GitHub CI builds ready-to-flash images for nodes 1, 2, and 3 in one artifact bundle.

## Raspberry Pi SocketCAN smoke test

The included tool uses only the Python standard library:

```bash
sudo ip link set can0 down 2>/dev/null || true
sudo ip link set can0 type can bitrate 500000 restart-ms 100
sudo ip link set can0 up
ip -details link show can0
```

Verify CANH, CANL, common ground, transceiver voltage, and exactly two 120 Ω terminators before powering the network.

Ping three nodes:

```bash
python3 tools/can_smoke_test.py --interface can0 ping 1
python3 tools/can_smoke_test.py --interface can0 ping 2
python3 tools/can_smoke_test.py --interface can0 ping 3
```

Apply commands:

```bash
python3 tools/can_smoke_test.py phase 1 146 3
python3 tools/can_smoke_test.py vga 1 23
python3 tools/can_smoke_test.py combined 1 64 4 12
python3 tools/can_smoke_test.py safe 1 3
```

The tool waits for a sequence-matched `ACK`, `ERROR`, or `STATUS` response and exits nonzero for errors or timeouts.

## Bench-test sequence

1. Test one receiver and the Raspberry Pi with termination at both ends.
2. Confirm repeated pings return stable node IDs and state.
3. Confirm phase-only and VGA-only commands produce acknowledgements and expected waveforms.
4. Confirm combined commands visibly apply maximum attenuation before the phase transition.
5. Add nodes 2 and 3 and confirm each ignores frames for the other nodes.
6. Send a broadcast safe command and confirm all boards enter safe output without transmitting responses.
7. Send invalid lengths and out-of-range values and confirm unicast `ERROR` responses.
8. Generate a controlled burst and verify status drop counters remain zero at the intended command rate.
9. Disconnect CANH/CANL temporarily and verify automatic bus-off recovery after restoring the bus.
10. Record firmware commit, node image, board revision, transceiver, connector pinout, termination, and captured traces.
