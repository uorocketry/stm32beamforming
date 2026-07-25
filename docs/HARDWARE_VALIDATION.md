# Hardware Validation Checklist

Complete this checklist for every PCB revision and every release artifact before enabling an operational RF path. Record the firmware Git revision, ELF SHA-256, board revision, board serial number, test equipment, operator, and date with the evidence.

## Identification

- [ ] Populated MCU marking confirms STM32F072R8T6.
- [ ] Schematic and PCB revision are recorded.
- [ ] Firmware revision and ELF SHA-256 are recorded.
- [ ] Power rails and logic levels match all connected devices.
- [ ] External pull resistors establish documented RF-safe reset states.

## Power and reset

- [ ] Test normal power ramp.
- [ ] Test slow power ramp.
- [ ] Test repeated power cycling.
- [ ] Test brownout and recovery.
- [ ] Confirm watchdog reset recovery.
- [ ] Confirm reset flags and retained diagnostics are readable after reset.
- [ ] Confirm corrupted or erased retained RAM is safely reinitialized.
- [ ] Confirm three incomplete boots cause safe lockout on the next boot.
- [ ] Confirm a newly flashed build clears the previous build's lockout streak.

## Clock behavior

- [ ] HSE starts and the system runs at 48 MHz.
- [ ] A missing or failed HSE causes the bounded HSI48 fallback.
- [ ] SPI clock frequencies are correct in both clock modes.
- [ ] Clock-security failure records a fault and resets the MCU.

## VGA transaction

Capture CS, SCK, and data using a logic analyzer or oscilloscope.

- [ ] CS is high while idle.
- [ ] SPI mode is CPOL=0, CPHA=0.
- [ ] Exactly eight clock edges occur.
- [ ] The 23 dB command is `0x5c`.
- [ ] Logical data order is LSB-first.
- [ ] CS setup, hold, and pulse timing meet the current F0480 datasheet.
- [ ] Measured attenuation agrees with the requested setting for every value from 0 through 23 dB.
- [ ] A forced pre-write timeout leaves CS high.
- [ ] A forced in-flight timeout does not intentionally latch partial data before reset.

## Phase-shifter transaction

Capture LE, SCK, data, and serial-programming select.

- [ ] Serial-programming select is high before programming.
- [ ] LE is high while idle and low during the transfer.
- [ ] SPI mode is CPOL=0, CPHA=0.
- [ ] Exactly 13 clock edges occur.
- [ ] The 205.3-degree, address-3 raw command is `0x092c`.
- [ ] The phase and address fields appear in the device-required logical order.
- [ ] OPT tracks phase-state bit D6 over representative states on both sides of each transition.
- [ ] LE setup, hold, and pulse timing meet the current PE44820 datasheet.
- [ ] Measured RF phase agrees with the requested state over all 256 states or an approved sampling plan.
- [ ] A forced pre-write timeout leaves LE high.
- [ ] A forced in-flight timeout does not intentionally latch partial data before reset.

## Fault injection

- [ ] Disconnect or hold each SPI clock/data line and verify bounded failure behavior.
- [ ] Force SPI status errors where practical and verify recorded fault codes.
- [ ] Trigger HardFault and verify reset plus retained fault history.
- [ ] Trigger an unused peripheral interrupt and verify reset plus retained fault history.
- [ ] Verify the watchdog is not refreshed in a fault path.

## Environmental and endurance testing

- [ ] Test at minimum and maximum qualified supply voltage.
- [ ] Test over the intended temperature range.
- [ ] Perform repeated startup and command cycles.
- [ ] Perform an EMI susceptibility test appropriate to the installation.
- [ ] Run a soak test for the intended deployment duration or an approved accelerated duration.

## Release approval

- [ ] All automated CI checks pass for the exact revision.
- [ ] The tested ELF checksum matches the archived release artifact.
- [ ] All deviations are documented and accepted by the responsible hardware and firmware reviewers.
- [ ] Rollback instructions and the previous qualified artifact are available.
