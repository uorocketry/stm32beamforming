#ifndef PHASESHIFTER_H
#define PHASESHIFTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/timer.h>
#include <include/PhaseStateEnum.h>


// RX4: each phase shifter has its own latch enable (LE).
// Data shifts in while LE is low; a high pulse on LE applies it.
// Pins from the RX4 MCU schematic. They count DOWN on purpose
// (PS1 = PB10, PS4 = PB0); this is not a typo.
#define SPI2_PS1_LE_PORT GPIOB
#define SPI2_PS1_LE_PIN GPIO10 

#define SPI2_PS2_LE_PORT GPIOB
#define SPI2_PS2_LE_PIN GPIO2 

#define SPI2_PS3_LE_PORT GPIOB
#define SPI2_PS3_LE_PIN GPIO1 

#define SPI2_PS4_LE_PORT GPIOB
#define SPI2_PS4_LE_PIN GPIO0 

// Serial/parallel select, shared by all 4 phase shifters.
// Must be HIGH so the chips are programmed over SPI (serial mode).
#define SPI2_PS_SP_PORT GPIOC
#define SPI2_PS_SP_PIN GPIO4

// SPI2 clock and data to the phase shifters.
// No MISO: the phase shifters never send data back, and on RX4
// PC11 (the old MISO pin) is wired to the power setpoints instead.
#define SPI2_PS_CLK_PORT GPIOB
#define SPI2_PS_CLK_PIN GPIO13

#define SPI2_PS_MOSI_PORT GPIOB
#define SPI2_PS_MOSI_PIN GPIO15

uint16_t reverseBits(uint16_t word, uint8_t numBits);
uint16_t MakePSCommand(optimizedPhaseState_e phaseState, uint8_t unitAddressWord);
void pe448spisetup(void);
#endif
