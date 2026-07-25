# STM32 Beamforming Firmware

Bare-metal C firmware for the STM32F072R8T6 receiver controller. The project uses libopencm3 and controls a PE44820 phase shifter and an F0480 variable-gain amplifier over separate write-only SPI buses.

## Current behavior

At startup the firmware:

1. Reads and clears the MCU reset flags.
2. Starts the independent watchdog.
3. Attempts to run from an 8 MHz HSE through the PLL at 48 MHz, with a bounded HSI48 fallback.
4. Initializes the VGA and phase-shifter interfaces.
5. Programs the VGA to 23 dB maximum attenuation.
6. Programs phase-shifter address `3` to the nearest state for `205.3°`.
7. Records the applied commands and enters a watchdog-serviced idle loop.

After three consecutive incomplete boots of the same firmware build, the controller enters a retained safe lockout. In lockout it applies maximum attenuation and phase state zero, then remains in the watchdog-serviced idle loop.

## Safety and correctness properties

- Phase angles use deterministic integer millidegree arithmetic.
- Phase addresses are restricted to four bits.
- The PE44820 OPT bit is derived from phase-state bit D6 rather than supplied independently.
- Phase commands are restricted to the 13-bit frame width.
- F0480 attenuation accepts every whole-decibel setting from 0 through 23 dB.
- VGA maximum attenuation is applied before the phase command.
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
```

Run the complete local quality gate when all dependencies are installed:

```bash
scripts/verify.sh
```

Build libopencm3 and the firmware:

```bash
make -C libopencm3 TARGETS=stm32/f0
make -C app clean all
arm-none-eabi-size app/build/stm32beamforming.elf
```

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
- runs the host protocol and diagnostic tests;
- runs cppcheck warning, style, performance, and portability analysis;
- builds libopencm3 and the STM32F072 firmware;
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

The exported `firmware_revision` string contains the Git revision supplied by the build. SRAM retention is best effort: power loss can erase the record, and corruption is rejected by its checksum.

## Repository structure

```text
app/include/beamforming_protocol.h  Hardware-independent command API
app/src/beamforming_protocol.c      PE44820 and F0480 command encoding
app/src/diagnostic_record.c         Tested retained-state logic
app/src/diagnostics.c                STM32 .noinit storage adapter
app/src/clock_control.c              Bounded clock setup and fallback
app/src/spi_guard.c                  Bounded SPI status polling
app/src/phaseShifter.c               SPI2 phase-shifter driver
app/src/vga.c                        SPI1 VGA driver
app/src/faults.c                     Reset-on-fault handlers
app/src/main.c                       Safe startup sequence
tests/                               Native C17 tests
```

The current firmware performs a fixed startup configuration. CAN and UART command processing should be added incrementally on top of this verified baseline rather than merged from unfinished development branches.
