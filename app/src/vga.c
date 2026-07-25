#include "Vga.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

static void control_line_timing_margin(void)
{
    __asm__ volatile("nop");
    __asm__ volatile("nop");
}

void vga_setup(void)
{
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_set(SPI1_VGA_CSB_PORT, SPI1_VGA_CSB_PIN);
    gpio_mode_setup(SPI1_VGA_CSB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI1_VGA_CSB_PIN);
    gpio_set_output_options(
        SPI1_VGA_CSB_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        SPI1_VGA_CSB_PIN);

    gpio_mode_setup(SPI1_VGA_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI1_VGA_CLK_PIN);
    gpio_mode_setup(SPI1_VGA_MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI1_VGA_MOSI_PIN);
    gpio_set_af(SPI1_VGA_CLK_PORT, GPIO_AF0, SPI1_VGA_CLK_PIN);
    gpio_set_af(SPI1_VGA_MOSI_PORT, GPIO_AF0, SPI1_VGA_MOSI_PIN);
    gpio_set_output_options(
        SPI1_VGA_CLK_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        SPI1_VGA_CLK_PIN | SPI1_VGA_MOSI_PIN);

    spi_disable(SPI1);
    spi_init_master(
        SPI1,
        SPI_CR1_BAUDRATE_FPCLK_DIV_16,
        SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
        SPI_CR1_CPHA_CLK_TRANSITION_1,
        SPI_CR1_LSBFIRST);
    spi_set_data_size(SPI1, SPI_CR2_DS_8BIT);
    spi_fifo_reception_threshold_8bit(SPI1);
    spi_set_bidirectional_transmit_only_mode(SPI1);
    spi_enable_software_slave_management(SPI1);
    spi_set_nss_high(SPI1);
    spi_enable(SPI1);
}

spi_guard_status_t vga_write(uint8_t command, uint32_t timeout_millis)
{
    spi_guard_status_t status = spi_guard_wait_txe(SPI1, timeout_millis);
    if (status != SPI_GUARD_OK) {
        return status;
    }

    gpio_clear(SPI1_VGA_CSB_PORT, SPI1_VGA_CSB_PIN);
    SPI_DR8(SPI1) = command;
    __asm__ volatile("dsb" ::: "memory");

    status = spi_guard_wait_complete(SPI1, timeout_millis);
    if (status != SPI_GUARD_OK) {
        return status;
    }

    control_line_timing_margin();
    gpio_set(SPI1_VGA_CSB_PORT, SPI1_VGA_CSB_PIN);
    control_line_timing_margin();
    return SPI_GUARD_OK;
}
