#include "PhaseShifter.h"
#include "Vga.h"
#include "beamforming_protocol.h"
#include "build_info.h"
#include "can_runtime.h"
#include "clock_control.h"
#include "diagnostics.h"
#include "faults.h"
#include "firmware_config.h"
#include "timebase.h"
#include "watchdog.h"

#include <libopencm3/stm32/rcc.h>

#include <stdbool.h>
#include <stdint.h>

#define WATCHDOG_TIMEOUT_MILLIS 1000u
#define SPI_TIMEOUT_MILLIS 2u
#define OPERATIONAL_PHASE_MILLIDEGREES 205300u
#define SAFE_PHASE_MILLIDEGREES 0u
#define PHASE_SHIFTER_ADDRESS 3u
#define SAFE_VGA_ATTENUATION_DB 23u
#define FIRMWARE_RESET_FLAG_MASK 0xfe000000u

static firmware_fault_t vga_fault_from_status(spi_guard_status_t status)
{
    return (status == SPI_GUARD_TIMEOUT)
        ? FIRMWARE_FAULT_VGA_SPI_TIMEOUT
        : FIRMWARE_FAULT_VGA_SPI_ERROR;
}

static firmware_fault_t phase_fault_from_status(spi_guard_status_t status)
{
    return (status == SPI_GUARD_TIMEOUT)
        ? FIRMWARE_FAULT_PHASE_SPI_TIMEOUT
        : FIRMWARE_FAULT_PHASE_SPI_ERROR;
}

static void service_without_can(void) __attribute__((noreturn));

static void service_without_can(void)
{
    while (1) {
        watchdog_service();
        __asm__ volatile("wfi");
    }
}

static void service_runtime(can_runtime_t *runtime) __attribute__((noreturn));

static void service_runtime(can_runtime_t *runtime)
{
    while (1) {
        bool processed_frame = false;
        while (can_runtime_service_next(runtime)) {
            processed_frame = true;
            watchdog_service();
        }

        watchdog_service();
        if (!processed_frame) {
            __asm__ volatile("wfi");
        }
    }
}

int main(void)
{
    const uint32_t reset_flags = RCC_CSR & FIRMWARE_RESET_FLAG_MASK;
    RCC_CSR |= RCC_CSR_RMVF;

    watchdog_start(WATCHDOG_TIMEOUT_MILLIS);
    diagnostics_begin(reset_flags, firmware_build_id());
    const bool safe_lockout = diagnostics_lockout_required();

    firmware_clock_t clock_source = FIRMWARE_CLOCK_UNKNOWN;
    if (!clock_control_setup(&clock_source)) {
        firmware_fail(FIRMWARE_FAULT_CLOCK_STARTUP);
    }

    timebase_setup(FIRMWARE_CORE_CLOCK_HZ);
    diagnostics_set_clock(clock_source);
    diagnostics_set_state(FIRMWARE_STATE_CLOCK_READY);

    vga_setup();
    phase_shifter_setup();

    uint8_t vga_command = 0u;
    if (!vga_command_from_attenuation(SAFE_VGA_ATTENUATION_DB, &vga_command)) {
        firmware_fail(FIRMWARE_FAULT_VGA_COMMAND);
    }

    const uint32_t requested_phase = safe_lockout
        ? SAFE_PHASE_MILLIDEGREES
        : OPERATIONAL_PHASE_MILLIDEGREES;
    uint8_t phase_state = 0u;
    uint16_t phase_command = 0u;
    if (!phase_state_from_millidegrees(requested_phase, &phase_state)
        || !phase_command_from_state(
            phase_state,
            PHASE_SHIFTER_ADDRESS,
            &phase_command)) {
        firmware_fail(FIRMWARE_FAULT_PHASE_COMMAND);
    }

    diagnostics_set_commands(phase_command, vga_command);

    spi_guard_status_t status = vga_write(vga_command, SPI_TIMEOUT_MILLIS);
    if (status != SPI_GUARD_OK) {
        firmware_fail(vga_fault_from_status(status));
    }
    diagnostics_set_state(FIRMWARE_STATE_SAFE_OUTPUTS);

    status = phase_shifter_write(phase_command, SPI_TIMEOUT_MILLIS);
    if (status != SPI_GUARD_OK) {
        firmware_fail(phase_fault_from_status(status));
    }

    const can_control_state_t initial_state = {
        .phase_state = phase_state,
        .phase_address = PHASE_SHIFTER_ADDRESS,
        .attenuation_db = SAFE_VGA_ATTENUATION_DB,
    };
    can_runtime_t runtime = {0};
    if (!can_runtime_start(
            &runtime,
            BEAMFORMER_NODE_ID,
            clock_source,
            safe_lockout,
            &initial_state,
            phase_command,
            vga_command)) {
        if (safe_lockout) {
            diagnostics_set_fault(FIRMWARE_FAULT_CAN_INIT);
            diagnostics_set_state(FIRMWARE_STATE_SAFE_LOCKOUT);
            service_without_can();
        }
        firmware_fail(FIRMWARE_FAULT_CAN_INIT);
    }

    if (safe_lockout) {
        diagnostics_set_state(FIRMWARE_STATE_SAFE_LOCKOUT);
    } else {
        diagnostics_mark_healthy();
    }

    service_runtime(&runtime);
}
