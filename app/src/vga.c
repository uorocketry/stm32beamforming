#include <include/Vga.h>

//See UnitTests -> RX1_UnitTest3

void f0480spisetup(void)
{
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_GPIOB); // CLK (PB3), MOSI (PB5)
    rcc_periph_clock_enable(RCC_GPIOC); // VGA1-4 CSB (PC0-PC3)

    // Each VGA has its own CSB (active low).
    // gpio_set comes BEFORE gpio_mode_setup on purpose: the pin is still an
    // input when we set it, so it comes up HIGH the moment it becomes an
    // output and never goes low by accident at startup.
    // F0480 datasheet: "When CSb is high, the CLK input is disabled and serial
    // data is not clocked into the shift register. It is recommended that CSb
    // be pulled high when the device is not being programmed."

    //VGA1
    gpio_set(SPI1_VGA1_CSB_PORT, SPI1_VGA1_CSB_PIN); // set CSB high. " When CSb is high (> VIH), the CLK input is disabled and serial 
                                                     // data (DATA) is not clocked into the shift register.  It is recommended that CSb be pulled high (>VIH) when 
                                                     // the device is not being programmed.""
    gpio_mode_setup(SPI1_VGA1_CSB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI1_VGA1_CSB_PIN);

    //VGA2
    gpio_set(SPI1_VGA2_CSB_PORT, SPI1_VGA2_CSB_PIN);
    gpio_mode_setup(SPI1_VGA2_CSB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI1_VGA2_CSB_PIN);

    //VGA3
    gpio_set(SPI1_VGA3_CSB_PORT, SPI1_VGA3_CSB_PIN);
    gpio_mode_setup(SPI1_VGA3_CSB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI1_VGA3_CSB_PIN);

    //VGA4
    gpio_set(SPI1_VGA4_CSB_PORT, SPI1_VGA4_CSB_PIN);
    gpio_mode_setup(SPI1_VGA4_CSB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI1_VGA4_CSB_PIN);

    // CLK (PB3) and MOSI (PB5) are driven by the SPI1 hardware: AF mode, AF0
    // (STM32F072 datasheet DS9826 Rev 6, Table 16).
    // No MISO: the VGAs never send data back.
    gpio_mode_setup(SPI1_VGA_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI1_VGA_CLK_PIN);
    gpio_mode_setup(SPI1_VGA_MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI1_VGA_MOSI_PIN);

    gpio_set_af(SPI1_VGA_CLK_PORT, GPIO_AF0, SPI1_VGA_CLK_PIN);
    gpio_set_af(SPI1_VGA_MOSI_PORT, GPIO_AF0, SPI1_VGA_MOSI_PIN);

    spi_set_data_size(SPI1, SPI_CR2_DS_8BIT);
    spi_fifo_reception_threshold_8bit(SPI1);
    spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_16, 0, 0, SPI_CR1_LSBFIRST);
}

