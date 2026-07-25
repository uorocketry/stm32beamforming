#include "diagnostic_record.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_new_record_is_valid(void)
{
    diagnostic_record_t record = diagnostic_record_new();
    assert(diagnostic_record_is_valid(&record));
    assert(record.state == FIRMWARE_STATE_UNINITIALIZED);
    assert(record.current_fault == FIRMWARE_FAULT_NONE);
}

static void test_corruption_is_rejected(void)
{
    diagnostic_record_t record = diagnostic_record_new();
    record.boot_count ^= 1u;
    assert(!diagnostic_record_is_valid(&record));
}

static void test_incomplete_boots_trigger_lockout(void)
{
    diagnostic_record_t record = diagnostic_record_new();

    for (uint32_t boot = 0; boot < DIAGNOSTIC_LOCKOUT_THRESHOLD; ++boot) {
        record = diagnostic_record_begin_boot(&record, boot, 0x12345678u);
        assert(diagnostic_record_is_valid(&record));
    }

    assert(record.consecutive_incomplete_boots == DIAGNOSTIC_LOCKOUT_THRESHOLD - 1u);
    record = diagnostic_record_begin_boot(&record, 99u, 0x12345678u);
    assert(diagnostic_record_should_lockout(&record));
}

static void test_healthy_boot_clears_failure_streak(void)
{
    diagnostic_record_t record = diagnostic_record_new();
    record = diagnostic_record_begin_boot(&record, 1u, 1u);
    record = diagnostic_record_begin_boot(&record, 2u, 1u);
    assert(record.consecutive_incomplete_boots == 1u);

    diagnostic_record_mark_healthy(&record);
    assert(record.consecutive_incomplete_boots == 0u);
    assert(record.state == FIRMWARE_STATE_HEALTHY);
    assert(diagnostic_record_is_valid(&record));

    record = diagnostic_record_begin_boot(&record, 3u, 1u);
    assert(record.consecutive_incomplete_boots == 0u);
}

static void test_new_build_clears_old_streak(void)
{
    diagnostic_record_t record = diagnostic_record_new();
    record = diagnostic_record_begin_boot(&record, 1u, 1u);
    record = diagnostic_record_begin_boot(&record, 2u, 1u);
    record = diagnostic_record_begin_boot(&record, 3u, 2u);

    assert(record.build_id == 2u);
    assert(record.boot_count == 1u);
    assert(record.consecutive_incomplete_boots == 0u);
}

static void test_fault_and_command_history(void)
{
    diagnostic_record_t record = diagnostic_record_new();
    record = diagnostic_record_begin_boot(&record, 0xa5u, 7u);

    diagnostic_record_set_clock(&record, FIRMWARE_CLOCK_HSI48);
    diagnostic_record_set_commands(&record, 0x092cu, 0x5cu);
    diagnostic_record_set_fault(&record, FIRMWARE_FAULT_PHASE_SPI_TIMEOUT);

    assert(record.clock_source == FIRMWARE_CLOCK_HSI48);
    assert(record.last_phase_command == 0x092cu);
    assert(record.last_vga_command == 0x5cu);
    assert(record.current_fault == FIRMWARE_FAULT_PHASE_SPI_TIMEOUT);
    assert(diagnostic_record_is_valid(&record));

    record = diagnostic_record_begin_boot(&record, 0u, 7u);
    assert(record.previous_fault == FIRMWARE_FAULT_PHASE_SPI_TIMEOUT);
    assert(record.current_fault == FIRMWARE_FAULT_NONE);
}

int main(void)
{
    test_new_record_is_valid();
    test_corruption_is_rejected();
    test_incomplete_boots_trigger_lockout();
    test_healthy_boot_clears_failure_streak();
    test_new_build_clears_old_streak();
    test_fault_and_command_history();

    puts("diagnostic tests passed");
    return 0;
}
