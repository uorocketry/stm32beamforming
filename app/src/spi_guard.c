#include "spi_guard.h"

#include "timebase.h"

#include <libopencm3/stm32/spi.h>

#include <stdbool.h>

#define SPI_POLL_ITERATION_LIMIT 1000000u

static bool spi_has_error(uint32_t spi)
{
    const uint32_t error_mask = SPI_SR_OVR | SPI_SR_MODF | SPI_SR_CRCERR;
    return (SPI_SR(spi) & error_mask) != 0u;
}

static spi_guard_status_t wait_for_status(
    uint32_t spi,
    uint32_t mask,
    bool expected_set,
    uint32_t timeout_millis)
{
    const uint32_t start = timebase_millis();

    for (uint32_t remaining = SPI_POLL_ITERATION_LIMIT; remaining > 0u; --remaining) {
        if (spi_has_error(spi)) {
            return SPI_GUARD_ERROR;
        }

        const bool is_set = (SPI_SR(spi) & mask) != 0u;
        if (is_set == expected_set) {
            return SPI_GUARD_OK;
        }

        if (timebase_elapsed(start, timeout_millis)) {
            return SPI_GUARD_TIMEOUT;
        }
    }

    return SPI_GUARD_TIMEOUT;
}

spi_guard_status_t spi_guard_wait_txe(uint32_t spi, uint32_t timeout_millis)
{
    return wait_for_status(spi, SPI_SR_TXE, true, timeout_millis);
}

spi_guard_status_t spi_guard_wait_not_busy(uint32_t spi, uint32_t timeout_millis)
{
    return wait_for_status(spi, SPI_SR_BSY, false, timeout_millis);
}

spi_guard_status_t spi_guard_wait_complete(uint32_t spi, uint32_t timeout_millis)
{
    const spi_guard_status_t txe_status = spi_guard_wait_txe(spi, timeout_millis);
    if (txe_status != SPI_GUARD_OK) {
        return txe_status;
    }

    return spi_guard_wait_not_busy(spi, timeout_millis);
}
