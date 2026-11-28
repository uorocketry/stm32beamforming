#include <include/PhaseShifter.h>
#include <include/PhaseStateEnum.h>
// PE44820 optimum-state lookup table at 2.4 GHz.
// Index = desired 8-bit phase state from CAN (0-255, 1.40625 deg steps).
// Value = 9-bit word to send (bit 8 = OPT bit, bits 7-0 = phase state) that
// gives the measured phase closest to the desired one.
// Source: PE44820_Lookup_3Feb2025.pdf, "Optimum State" column. Extracted by
// script; checked 256 rows in order, each value matches its 9-bit binary column.
// Only valid at 2.4 GHz: a different frequency needs a new table.
// const: never changes, so it lives in flash (512 bytes) instead of RAM.
const uint16_t ps_optimum_state[256] = {
    0, 257, 2, 259, 4, 261, 6, 7, // states 0-7
    263, 8, 9, 10, 11, 267, 12, 13, // states 8-15
    16, 17, 18, 275, 20, 21, 22, 23, // states 16-23
    279, 24, 25, 26, 27, 283, 28, 29, // states 24-31
    32, 33, 287, 35, 36, 37, 38, 294, // states 32-39
    295, 40, 296, 297, 42, 299, 44, 45, // states 40-47
    301, 304, 303, 306, 307, 308, 309, 310, // states 48-55
    311, 56, 312, 313, 314, 315, 60, 61, // states 56-63
    320, 318, 322, 323, 68, 325, 70, 327, // states 64-71
    327, 72, 328, 74, 75, 331, 76, 77, // states 72-79
    80, 337, 82, 339, 84, 340, 86, 87, // states 80-87
    343, 88, 89, 90, 91, 347, 92, 93, // states 88-95
    96, 353, 351, 355, 100, 101, 357, 103, // states 96-103
    359, 104, 105, 361, 107, 363, 363, 108, // states 104-111
    364, 368, 114, 367, 371, 116, 373, 118, // states 112-119
    375, 120, 121, 377, 122, 379, 379, 124, // states 120-127
    125, 381, 126, 128, 385, 130, 387, 132, // states 128-135
    389, 134, 135, 391, 136, 137, 138, 139, // states 136-143
    395, 140, 141, 144, 398, 146, 147, 148, // states 144-151
    404, 405, 406, 407, 152, 153, 154, 155, // states 152-159
    411, 156, 157, 160, 159, 162, 419, 164, // states 160-167
    421, 166, 167, 423, 168, 169, 170, 426, // states 168-175
    427, 172, 429, 174, 175, 178, 179, 180, // states 176-183
    181, 182, 183, 439, 184, 185, 186, 187, // states 184-191
    443, 188, 445, 190, 449, 194, 451, 196, // states 192-199
    197, 453, 199, 455, 200, 456, 457, 202, // states 200-207
    459, 459, 204, 205, 209, 210, 463, 467, // states 208-215
    212, 469, 214, 471, 216, 216, 473, 218, // states 216-223
    475, 475, 220, 221, 480, 478, 482, 483, // states 224-231
    228, 484, 230, 231, 487, 232, 233, 489, // states 232-239
    235, 491, 491, 236, 237, 241, 242, 499, // states 240-247
    495, 244, 501, 246, 503, 248, 249, 505, // states 248-255
};
//See UnitTests -> RX1_UnitTest 2

// The phase word and address field need bit order reversed before packing, see datasheet timing diagram and command structure.
uint16_t reverseBits(uint16_t word, uint8_t numBits)
{
    uint16_t reversed = 0;

    for (uint8_t i = 0; i < numBits; i++) {
        reversed = (uint16_t)((reversed << 1) | (word & 0x1u));
        word >>= 1;
    }

    return reversed;
}

uint16_t MakePSCommand(uint16_t stateword, uint8_t unitAddressWord)
{
    //[reversedStateWOrd][optBit][reversedAddress]
    
    bool optBit = (stateword & 0b100000000); 
    uint8_t stateWordMasked = stateword & (0b011111111); 
    stateWordMasked = reverseBits((uint16_t)stateWordMasked, 8); // reverse the 8-bit phase field
    unitAddressWord = reverseBits(unitAddressWord, 4); //default to using the first address,
    uint16_t command =((stateWordMasked & 0xFFu) << 5) | ((optBit & 0x1u) << 4) |(unitAddressWord & 0x0Fu);

    return command;
} 

void pe448spisetup(void)
{
    rcc_periph_clock_enable(RCC_SPI2);
    rcc_periph_clock_enable(RCC_GPIOB); // CLK (PB13), MOSI (PB15), LE1-4 (PB10, PB2, PB1, PB0)
    rcc_periph_clock_enable(RCC_GPIOC); // S/P (PC4)

    // S/P must be HIGH for serial mode. Set it before making it an output
    // so it never briefly selects parallel mode at startup.
    gpio_set(SPI2_PS_SP_PORT, SPI2_PS_SP_PIN);
    gpio_mode_setup(SPI2_PS_SP_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS_SP_PIN);

    // CLK (PB13) and MOSI (PB15) are driven by the SPI2 hardware: AF mode, AF0
    // (STM32F072 datasheet DS9826 Rev 6, Table 16).
    // No MISO: the phase shifters never send data back.
    gpio_mode_setup(SPI2_PS_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI2_PS_CLK_PIN);
    gpio_mode_setup(SPI2_PS_MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI2_PS_MOSI_PIN);
    gpio_set_af(SPI2_PS_CLK_PORT, GPIO_AF0, SPI2_PS_CLK_PIN);
    gpio_set_af(SPI2_PS_MOSI_PORT, GPIO_AF0, SPI2_PS_MOSI_PIN);

    // Each phase shifter has its own LE. They must start LOW: a rising edge
    // on LE applies whatever is in the chip's shift register, which is
    // garbage at power-up. Clear before making them outputs, same reason as S/P.

    //PS1
    gpio_clear(SPI2_PS1_LE_PORT, SPI2_PS1_LE_PIN);
    gpio_mode_setup(SPI2_PS1_LE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS1_LE_PIN);

    //PS2
    gpio_clear(SPI2_PS2_LE_PORT, SPI2_PS2_LE_PIN);
    gpio_mode_setup(SPI2_PS2_LE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS2_LE_PIN);

    //PS3
    gpio_clear(SPI2_PS3_LE_PORT, SPI2_PS3_LE_PIN);
    gpio_mode_setup(SPI2_PS3_LE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS3_LE_PIN);

    //PS4
    gpio_clear(SPI2_PS4_LE_PORT, SPI2_PS4_LE_PIN);
    gpio_mode_setup(SPI2_PS4_LE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS4_LE_PIN);

    
    spi_set_data_size(SPI2, SPI_CR2_DS_13BIT); //our command is 13 bits long.
    spi_fifo_reception_threshold_16bit(SPI2);
    spi_init_master(SPI2, SPI_CR1_BAUDRATE_FPCLK_DIV_16, 0, 0, SPI_CR1_MSBFIRST); //send MSB first. We've already flipped the command elements to LSB first as required during command construction. 
}
