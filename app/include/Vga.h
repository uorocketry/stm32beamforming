#ifndef VGA_H
#define VGA_H
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <stdint.h>

// RX4: each VGA has its own chip select (CSB), active low.
// Pull a VGA's CSB low to send it a byte, then set it back high.
// Pins from the RX4 MCU schematic.
#define SPI1_VGA1_CSB_PORT  GPIOC
#define SPI1_VGA1_CSB_PIN   GPIO0

#define SPI1_VGA2_CSB_PORT  GPIOC
#define SPI1_VGA2_CSB_PIN   GPIO1

#define SPI1_VGA3_CSB_PORT  GPIOC
#define SPI1_VGA3_CSB_PIN   GPIO2

#define SPI1_VGA4_CSB_PORT  GPIOC
#define SPI1_VGA4_CSB_PIN   GPIO3

#define SPI1_VGA_CLK_PORT  GPIOB
#define SPI1_VGA_CLK_PIN   GPIO3

// STBY (PB6) is intentionally not defined or driven.
// All 4 VGA STBY pins share this one net, and the F0480 has an
// internal ~1M pull-down, so leaving PB6 alone keeps the VGAs on.
// Only drive it if we ever need to turn all VGAs off.
#define SPI1_VGA_MOSI_PORT GPIOB
#define SPI1_VGA_MOSI_PIN  GPIO5

#define VGA_MAX_ATTEN_BYTE 92

// max serial clock speed for vga is 10 mhz
// serial control mode
// data clocked in LSB first 

// example
// for 8 db attenuation we have
// D0 = x
// D1 = x
// D2 = L
// D3 = L
// D4 = L
// D5 = H
// D6 = L
// D7 = x

// for max attenuation (23db) we have

// D0 = x
// D1 = x
// D2 = H
// D3 = H
// D4 = H
// D5 = L
// D6 = H
// D7 = x

typedef enum 
{
    ATTEN_0DB  = 0b00000000,
    ATTEN_1DB  = 0b00000100,
    ATTEN_2DB  = 0b00001000,
    ATTEN_4DB  = 0b00010000,
    ATTEN_8DB  = 0b00100000,
    ATTEN_16DB = 0b01000000,
    ATTEN_22DB = 0b01011000,
    ATTEN_23DB = 0b01011100
} SupportedAttenuationCommand_e;

typedef SupportedAttenuationCommand_e SupportedAttenuationCommand_t;

void f0480spisetup(void);

#endif
