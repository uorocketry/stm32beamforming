#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "diagnostic_record.h"

#include <stdbool.h>
#include <stdint.h>

extern volatile diagnostic_record_t FIRMWARE_DIAGNOSTICS;

void diagnostics_begin(uint32_t reset_flags, uint32_t build_id);
bool diagnostics_lockout_required(void);
void diagnostics_set_state(firmware_state_t state);
void diagnostics_set_clock(firmware_clock_t clock_source);
void diagnostics_set_commands(uint16_t phase_command, uint8_t vga_command);
void diagnostics_set_fault(firmware_fault_t fault);
void diagnostics_mark_healthy(void);
diagnostic_record_t diagnostics_snapshot(void);

#endif
