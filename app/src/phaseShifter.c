#include <include/PhaseShifter.h>
#include <include/PhaseStateEnum.h>

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

uint16_t MakePSCommand(optimizedPhaseState_e stateword, uint8_t unitAddressWord)
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
