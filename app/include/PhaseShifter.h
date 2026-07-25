#ifndef PHASE_SHIFTER_H
#define PHASE_SHIFTER_H

#include "spi_guard.h"

#include <stdint.h>

#define SPI2_PS_LE_PORT GPIOB
#define SPI2_PS_LE_PIN GPIO12
#define SPI2_PS_SP_PORT GPIOC
#define SPI2_PS_SP_PIN GPIO10
#define SPI2_PS_CLK_PORT GPIOB
#define SPI2_PS_CLK_PIN GPIO13
#define SPI2_PS_MOSI_PORT GPIOB
#define SPI2_PS_MOSI_PIN GPIO15

void phase_shifter_setup(void);
spi_guard_status_t phase_shifter_write(uint16_t command, uint32_t timeout_millis);

#endif
