#include "diagnostic_record.h"

#include <stddef.h>

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

static uint32_t checksum_mix_byte(uint32_t hash, uint8_t value)
{
    return (hash ^ value) * FNV_PRIME;
}

static uint32_t checksum_mix_u16(uint32_t hash, uint16_t value)
{
    hash = checksum_mix_byte(hash, (uint8_t)value);
    return checksum_mix_byte(hash, (uint8_t)(value >> 8u));
}

static uint32_t checksum_mix_u32(uint32_t hash, uint32_t value)
{
    hash = checksum_mix_u16(hash, (uint16_t)value);
    return checksum_mix_u16(hash, (uint16_t)(value >> 16u));
}

static uint32_t diagnostic_record_checksum(const diagnostic_record_t *record)
{
    uint32_t hash = FNV_OFFSET_BASIS;

    hash = checksum_mix_u32(hash, record->magic);
    hash = checksum_mix_u32(hash, record->version);
    hash = checksum_mix_u32(hash, record->build_id);
    hash = checksum_mix_u32(hash, record->boot_count);
    hash = checksum_mix_u32(hash, record->reset_flags);
    hash = checksum_mix_u16(hash, record->current_fault);
    hash = checksum_mix_u16(hash, record->previous_fault);
    hash = checksum_mix_u16(hash, record->last_phase_command);
    hash = checksum_mix_byte(hash, record->last_vga_command);
    hash = checksum_mix_byte(hash, record->state);
    hash = checksum_mix_byte(hash, record->clock_source);
    hash = checksum_mix_byte(hash, record->consecutive_incomplete_boots);

    return hash;
}

static void diagnostic_record_seal(diagnostic_record_t *record)
{
    if (record != NULL) {
        record->checksum = diagnostic_record_checksum(record);
    }
}

diagnostic_record_t diagnostic_record_new(void)
{
    diagnostic_record_t record = {
        .magic = DIAGNOSTIC_MAGIC,
        .version = DIAGNOSTIC_VERSION,
        .build_id = 0u,
        .boot_count = 0u,
        .reset_flags = 0u,
        .checksum = 0u,
        .current_fault = FIRMWARE_FAULT_NONE,
        .previous_fault = FIRMWARE_FAULT_NONE,
        .last_phase_command = 0u,
        .last_vga_command = 0u,
        .state = FIRMWARE_STATE_UNINITIALIZED,
        .clock_source = FIRMWARE_CLOCK_UNKNOWN,
        .consecutive_incomplete_boots = 0u,
    };

    diagnostic_record_seal(&record);
    return record;
}

bool diagnostic_record_is_valid(const diagnostic_record_t *record)
{
    return (record != NULL)
        && (record->magic == DIAGNOSTIC_MAGIC)
        && (record->version == DIAGNOSTIC_VERSION)
        && (record->checksum == diagnostic_record_checksum(record));
}

diagnostic_record_t diagnostic_record_begin_boot(
    const diagnostic_record_t *previous,
    uint32_t reset_flags,
    uint32_t build_id)
{
    diagnostic_record_t record = diagnostic_record_new();

    if (diagnostic_record_is_valid(previous) && (previous->build_id == build_id)) {
        record = *previous;
        record.previous_fault = record.current_fault;
        record.current_fault = FIRMWARE_FAULT_NONE;

        if (record.state == FIRMWARE_STATE_HEALTHY) {
            record.consecutive_incomplete_boots = 0u;
        } else if (record.consecutive_incomplete_boots < UINT8_MAX) {
            ++record.consecutive_incomplete_boots;
        }
    }

    record.build_id = build_id;
    ++record.boot_count;
    record.reset_flags = reset_flags;
    record.state = FIRMWARE_STATE_BOOTING;
    record.clock_source = FIRMWARE_CLOCK_UNKNOWN;
    diagnostic_record_seal(&record);
    return record;
}

bool diagnostic_record_should_lockout(const diagnostic_record_t *record)
{
    return diagnostic_record_is_valid(record)
        && (record->consecutive_incomplete_boots >= DIAGNOSTIC_LOCKOUT_THRESHOLD);
}

void diagnostic_record_set_state(diagnostic_record_t *record, firmware_state_t state)
{
    if (record != NULL) {
        record->state = (uint8_t)state;
        diagnostic_record_seal(record);
    }
}

void diagnostic_record_set_clock(diagnostic_record_t *record, firmware_clock_t clock_source)
{
    if (record != NULL) {
        record->clock_source = (uint8_t)clock_source;
        diagnostic_record_seal(record);
    }
}

void diagnostic_record_set_commands(
    diagnostic_record_t *record,
    uint16_t phase_command,
    uint8_t vga_command)
{
    if (record != NULL) {
        record->last_phase_command = phase_command;
        record->last_vga_command = vga_command;
        diagnostic_record_seal(record);
    }
}

void diagnostic_record_set_fault(diagnostic_record_t *record, firmware_fault_t fault)
{
    if (record != NULL) {
        record->current_fault = (uint16_t)fault;
        record->state = FIRMWARE_STATE_FAULT;
        diagnostic_record_seal(record);
    }
}

void diagnostic_record_mark_healthy(diagnostic_record_t *record)
{
    if (record != NULL) {
        record->state = FIRMWARE_STATE_HEALTHY;
        record->consecutive_incomplete_boots = 0u;
        diagnostic_record_seal(record);
    }
}
