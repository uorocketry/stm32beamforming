# STM32 Beamforming Firmware

Bare-metal C firmware for an STM32F072R8T6 receiver-chain controller. The project uses libopencm3 to control a PE44820 phase shifter, an F0480 variable-gain amplifier, and a multi-node classic CAN bus.

## Current behavior

At startup the firmware:

1. Reads and clears the MCU reset flags.
2. Starts the independent watchdog.
3. Attempts to run from an 8 MHz HSE through the PLL at 48 MHz, with a bounded HSI48 fallback.
4. Initializes the VGA and phase-shifter interfaces.
5. Programs the VGA to 23 dB maximum attenuation.
6. Programs phase-shifter address `3` to the nearest state for `205.3°`.
7. Initializes CAN1 at 500 kbit/s for the compiled receiver node ID.
8. Records the applied commands and enters an interrupt-driven, watchdog-serviced CAN command loop.

After three consecutive incomplete boots of the same firmware build, the controller enters a retained safe lockout. In lockout it applies maximum attenuation and phase state zero, answers unicast status requests, and rejects output-changing commands other than the idempotent safe command.

## Safety and correctness properties

- Phase angles use deterministic integer millidegree arithmetic.
- Phase addresses are restricted to four bits.
- The PE44820 OPT bit is derived from phase-state bit D6 rather than supplied independently.
- Phase commands are restricted to the 13-bit frame width.
- F0480 attenuation accepts every whole-decibel setting from 0 through 23 dB.
- VGA maximum attenuation is applied before the phase command.
- Phase updates temporarily apply maximum attenuation and then restore the requested or previous attenuation.
- Receiver boards accept output commands only from controller node `0`.
- Broadcast commands never generate acknowledgements, preventing response storms.
- CAN hardware filters accept only extended data frames addressed to the local node or broadcast.
- The CAN ISR drains the three-frame hardware FIFO into an eight-frame single-producer/single-consumer queue.
- CAN payload parsing, destination handling, response construction, filter encoding, queue behavior, and output ordering have native host tests.
- SPI TXE and BSY waits have millisecond and iteration limits.
- A transfer that times out before writing leaves the control line idle. A timeout after writing begins keeps LE/CS asserted so incomplete shift-register data is not deliberately latched before reset.
- HSE, PLL, and HSI48 startup waits are bounded.
- Clock-security failure, HardFault, and unexpected peripheral interrupts are recorded before reset.
- Compiler warnings are errors in both local and CI firmware builds.

Software cannot guarantee RF-safe levels during power ramp or before GPIO initialization. The PCB must provide appropriate passive pull resistors, and deployment requires the checks in [`docs/HARDWARE_VALIDATION.md`](docs/HARDWARE_VALIDATION.md).

## Hardware target

| Property | Value |
|---|---|
| MCU | STM32F072R8T6 |
| Core | Arm Cortex-M0 |
| Flash | 64 KiB |
| RAM | 16 KiB |
| Primary clock | 8 MHz HSE, PLL to 48 MHz |
| Fallback clock | 48 MHz HSI48 |

### Phase shifter

| Signal | MCU pin |
|---|---:|
| Latch enable | PB12 |
| SPI2 clock | PB13, AF0 |
| SPI2 data output | PB15, AF0 |
| Serial programming select | PC10 |

SPI2 uses mode 0, a 13-bit frame, and a 3 MHz clock at the 48 MHz peripheral clock. The encoded phase and address fields are bit-reversed before the MCU shifts the raw frame MSB-first.

### VGA

| Signal | MCU pin |
|---|---:|
| Chip select | PA4 |
| SPI1 clock | PA5, AF0 |
| SPI1 data output | PA7, AF0 |

SPI1 uses mode 0, eight-bit transfers, logical LSB-first data, and a 3 MHz clock.

### CAN

| Signal | MCU pin |
|---|---:|
| CAN1 RX | PA11, AF4 |
| CAN1 TX | PA12, AF4 |

CAN1 uses classic CAN 2.0B extended frames at 500 kbit/s through an external 3.3 V transceiver. The tested protocol supports controller node `0`, receiver nodes `1..30`, and broadcast destination `31`. See [`docs/CAN_PROTOCOL.md`](docs/CAN_PROTOCOL.md) for the identifier layout, payloads, responses, and Raspberry Pi SocketCAN procedure.

## Clone

```bash
git clone --recurse-submodules https://github.com/uorocketry/stm32beamforming.git
cd stm32beamforming
```

For an existing clone:

```bash
git submodule update --init --recursive
```

## Dependencies

- GNU Make
- Python 3
- ARM GNU Embedded Toolchain providing `arm-none-eabi-gcc`, `objcopy`, and `size`
- Newlib for Arm
- A native C17 compiler for host tests
- cppcheck for the same static-analysis pass used by CI

The required libopencm3 revision is pinned as a Git submodule.

## Test and build

Run the hardware-independent tests:

```bash
make -C tests clean test
python3 tests/test_can_smoke_tool.py
```

Run the complete local quality gate when all dependencies are installed:

```bash
scripts/verify.sh
```

Build libopencm3 and the firmware:

```bash
make -C libopencm3 TARGETS=stm32/f0
make -C app clean all CAN_NODE_ID=1
arm-none-eabi-size app/build/stm32beamforming.elf
```

`CAN_NODE_ID` must be in the range `1..30`. Build a distinct image for each receiver board. GitHub CI produces ready-to-flash images for nodes 1, 2, and 3.

Generated output is written under `app/build/`:

- `stm32beamforming.elf`
- `stm32beamforming.bin`
- `stm32beamforming.map`
- compiler stack-usage files

The generated device linker script is intentionally not tracked.

## Continuous integration

`.github/workflows/firmware-ci.yml` runs on every push and pull request. It:

- checks out all submodules;
- installs the ARM GNU toolchain;
- runs the host phase/VGA, diagnostic, CAN protocol, queue, filter, output-order, and SocketCAN-tool tests;
- repeats all native C tests under AddressSanitizer and UndefinedBehaviorSanitizer;
- runs cppcheck warning, style, performance, and portability analysis;
- builds libopencm3 and STM32F072 firmware images for receiver nodes 1, 2, and 3;
- treats all firmware warnings as errors;
- records the image size; and
- uploads the ELF, BIN, MAP, and size report.

After this workflow is established on `main`, repository administrators should make `firmware-ci / verify-and-build` a required branch-protection check.

## Retained diagnostics

The exported `FIRMWARE_DIAGNOSTICS` symbol is stored in the linker script's `.noinit` RAM section. Its checksummed record includes:

- firmware build ID;
- boot count and consecutive incomplete boots;
- RCC reset flags;
- current and previous fault codes;
- startup state and active clock source;
- last phase-shifter command; and
- last VGA command.

The exported `firmware_revision` string contains the Git revision supplied by the build, and `firmware_node_id` contains the compiled receiver ID. The retained build identity hashes both values, so flashing a different node image does not inherit the previous image's incomplete-boot lockout streak. SRAM retention is best effort: power loss can erase the record, and corruption is rejected by its checksum.

## Repository structure

```text
app/include/beamforming_protocol.h  Hardware-independent command API
app/src/beamforming_protocol.c      PE44820 and F0480 command encoding
app/src/can_protocol.c              Versioned CAN identifier and payload codec
app/src/can_filter.c                Tested bxCAN acceptance-filter encoding
app/src/can_queue.c                 ISR-to-main receive queue
app/src/can_control.c               Tested safe hardware-operation planning
app/src/can_bus.c                   CAN1 pins, timing, filters, ISR, and transport
app/src/can_runtime.c               Command execution, ACK/ERROR, and status service
app/src/diagnostic_record.c         Tested retained-state logic
app/src/diagnostics.c                STM32 .noinit storage adapter
app/src/clock_control.c              Bounded clock setup and fallback
app/src/spi_guard.c                  Bounded SPI status polling
app/src/phaseShifter.c               SPI2 phase-shifter driver
app/src/vga.c                        SPI1 VGA driver
app/src/faults.c                     Reset-on-fault handlers
app/src/main.c                       Safe startup sequence
tests/                               Native C17 tests
tools/can_smoke_test.py              Dependency-free Raspberry Pi SocketCAN client
```

This repository intentionally contains only per-board firmware. Beam steering, multi-board coordination, calibration strategy, and higher-level control algorithms should live in a separate controller repository that communicates through the documented CAN protocol.
