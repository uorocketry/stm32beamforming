#include "beamforming_protocol.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static void test_known_phase_command(void)
{
    uint8_t state = 0u;
    assert(phase_state_from_millidegrees(205300u, &state));
    assert(state == 146u);

    uint16_t command = 0;
    assert(phase_command_from_millidegrees(205300u, 3u, &command));
    assert(command == 0x092cu);
}

static void test_phase_boundaries(void)
{
    uint16_t command = 0xffffu;

    assert(phase_command_from_millidegrees(0u, 3u, &command));
    assert(command == 0x000cu);

    assert(phase_command_from_millidegrees(360000u, 3u, &command));
    assert(command == 0x000cu);

    assert(!phase_command_from_millidegrees(360001u, 3u, &command));
    assert(!phase_command_from_millidegrees(0u, 16u, &command));
    assert(!phase_command_from_millidegrees(0u, 0u, NULL));

    uint8_t state = 0xffu;
    assert(phase_state_from_millidegrees(360000u, &state));
    assert(state == 0u);
    assert(!phase_state_from_millidegrees(360001u, &state));
    assert(!phase_state_from_millidegrees(0u, NULL));
}

static void test_every_phase_state(void)
{
    for (uint16_t state = 0; state < PHASE_STATE_COUNT; ++state) {
        uint16_t command = 0;
        assert(phase_command_from_state((uint8_t)state, 0x0fu, &command));
        assert(command <= PHASE_COMMAND_MAX);

        const bool expected_option = (state & PHASE_OPTION_STATE_BIT) != 0u;
        const bool encoded_option = (command & PHASE_COMMAND_OPTION_MASK) != 0u;
        assert(expected_option == encoded_option);
    }
}

static void test_bit_reversal(void)
{
    assert(beamforming_reverse_bits(0x92u, 8u) == 0x49u);
    assert(beamforming_reverse_bits(0x03u, 4u) == 0x0cu);
    assert(beamforming_reverse_bits(0xffffu, 0u) == 0u);
}

static void test_vga_commands(void)
{
    for (uint8_t attenuation = 0; attenuation <= VGA_MAX_ATTENUATION_DB; ++attenuation) {
        uint8_t command = 0;
        assert(vga_command_from_attenuation(attenuation, &command));
        assert(command == (uint8_t)(attenuation << VGA_ATTENUATION_SHIFT));
    }

    uint8_t command = 0;
    assert(vga_command_from_attenuation(23u, &command));
    assert(command == 0x5cu);
    assert(!vga_command_from_attenuation(24u, &command));
    assert(!vga_command_from_attenuation(0u, NULL));
}

int main(void)
{
    test_known_phase_command();
    test_phase_boundaries();
    test_every_phase_state();
    test_bit_reversal();
    test_vga_commands();

    puts("protocol tests passed");
    return 0;
}
