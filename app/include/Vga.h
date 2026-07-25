#ifndef VGA_H
#define VGA_H

#include "spi_guard.h"

#include <stdint.h>

#define SPI1_VGA_CSB_PORT GPIOA
#define SPI1_VGA_CSB_PIN GPIO4
#define SPI1_VGA_CLK_PORT GPIOA
#define SPI1_VGA_CLK_PIN GPIO5
#define SPI1_VGA_MOSI_PORT GPIOA
#define SPI1_VGA_MOSI_PIN GPIO7

void vga_setup(void);
spi_guard_status_t vga_write(uint8_t command, uint32_t timeout_millis);

#endif
