#ifndef SPI_GUARD_H
#define SPI_GUARD_H

#include <stdint.h>

typedef enum spi_guard_status {
    SPI_GUARD_OK = 0,
    SPI_GUARD_TIMEOUT = 1,
    SPI_GUARD_ERROR = 2,
    SPI_GUARD_INVALID_ARGUMENT = 3
} spi_guard_status_t;

spi_guard_status_t spi_guard_wait_txe(uint32_t spi, uint32_t timeout_millis);
spi_guard_status_t spi_guard_wait_not_busy(uint32_t spi, uint32_t timeout_millis);
spi_guard_status_t spi_guard_wait_complete(uint32_t spi, uint32_t timeout_millis);

#endif
