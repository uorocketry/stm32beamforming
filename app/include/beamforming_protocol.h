#ifndef BEAMFORMING_PROTOCOL_H
#define BEAMFORMING_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define PHASE_STATE_COUNT 256u
#define PHASE_FULL_TURN_MILLIDEGREES 360000u
#define PHASE_OPTION_STATE_BIT 0x40u
#define PHASE_COMMAND_OPTION_MASK 0x0010u
#define PHASE_COMMAND_MAX 0x1fffu

#define VGA_MAX_ATTENUATION_DB 23u
#define VGA_ATTENUATION_SHIFT 2u

uint16_t beamforming_reverse_bits(uint16_t word, uint8_t bit_count);
bool phase_state_from_millidegrees(
    uint32_t requested_shift_millidegrees,
    uint8_t *phase_state);
bool phase_command_from_state(uint8_t phase_state, uint8_t unit_address, uint16_t *command);
bool phase_command_from_millidegrees(
    uint32_t requested_shift_millidegrees,
    uint8_t unit_address,
    uint16_t *command);
bool vga_command_from_attenuation(uint8_t attenuation_db, uint8_t *command);

#endif
