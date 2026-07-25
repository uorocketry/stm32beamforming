#include "diagnostics.h"

volatile diagnostic_record_t FIRMWARE_DIAGNOSTICS
    __attribute__((section(".noinit.diagnostics"), used));

static void diagnostics_store(diagnostic_record_t record)
{
    FIRMWARE_DIAGNOSTICS = record;
}

static diagnostic_record_t diagnostics_valid_snapshot(void)
{
    diagnostic_record_t record = diagnostics_snapshot();
    if (!diagnostic_record_is_valid(&record)) {
        record = diagnostic_record_new();
    }

    return record;
}

diagnostic_record_t diagnostics_snapshot(void)
{
    return FIRMWARE_DIAGNOSTICS;
}

void diagnostics_begin(uint32_t reset_flags, uint32_t build_id)
{
    const diagnostic_record_t previous = diagnostics_snapshot();
    diagnostics_store(diagnostic_record_begin_boot(&previous, reset_flags, build_id));
}

bool diagnostics_lockout_required(void)
{
    const diagnostic_record_t record = diagnostics_snapshot();
    return diagnostic_record_should_lockout(&record);
}

void diagnostics_set_state(firmware_state_t state)
{
    diagnostic_record_t record = diagnostics_valid_snapshot();
    diagnostic_record_set_state(&record, state);
    diagnostics_store(record);
}

void diagnostics_set_clock(firmware_clock_t clock_source)
{
    diagnostic_record_t record = diagnostics_valid_snapshot();
    diagnostic_record_set_clock(&record, clock_source);
    diagnostics_store(record);
}

void diagnostics_set_commands(uint16_t phase_command, uint8_t vga_command)
{
    diagnostic_record_t record = diagnostics_valid_snapshot();
    diagnostic_record_set_commands(&record, phase_command, vga_command);
    diagnostics_store(record);
}

void diagnostics_set_fault(firmware_fault_t fault)
{
    diagnostic_record_t record = diagnostics_valid_snapshot();
    diagnostic_record_set_fault(&record, fault);
    diagnostics_store(record);
}

void diagnostics_mark_healthy(void)
{
    diagnostic_record_t record = diagnostics_valid_snapshot();
    diagnostic_record_mark_healthy(&record);
    diagnostics_store(record);
}
