#include "PhaseShifter.h"

#include "beamforming_protocol.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

static void control_line_timing_margin(void)
{
    __asm__ volatile("nop");
    __asm__ volatile("nop");
}

void phase_shifter_setup(void)
{
    rcc_periph_clock_enable(RCC_SPI2);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    gpio_set(SPI2_PS_LE_PORT, SPI2_PS_LE_PIN);
    gpio_mode_setup(SPI2_PS_LE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS_LE_PIN);
    gpio_set_output_options(
        SPI2_PS_LE_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        SPI2_PS_LE_PIN);

    gpio_set(SPI2_PS_SP_PORT, SPI2_PS_SP_PIN);
    gpio_mode_setup(SPI2_PS_SP_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI2_PS_SP_PIN);
    gpio_set_output_options(
        SPI2_PS_SP_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        SPI2_PS_SP_PIN);

    gpio_mode_setup(SPI2_PS_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI2_PS_CLK_PIN);
    gpio_mode_setup(SPI2_PS_MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI2_PS_MOSI_PIN);
    gpio_set_af(SPI2_PS_CLK_PORT, GPIO_AF0, SPI2_PS_CLK_PIN);
    gpio_set_af(SPI2_PS_MOSI_PORT, GPIO_AF0, SPI2_PS_MOSI_PIN);
    gpio_set_output_options(
        SPI2_PS_CLK_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        SPI2_PS_CLK_PIN | SPI2_PS_MOSI_PIN);

    spi_disable(SPI2);
    spi_init_master(
        SPI2,
        SPI_CR1_BAUDRATE_FPCLK_DIV_16,
        SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
        SPI_CR1_CPHA_CLK_TRANSITION_1,
        SPI_CR1_MSBFIRST);
    spi_set_data_size(SPI2, SPI_CR2_DS_13BIT);
    spi_fifo_reception_threshold_16bit(SPI2);
    spi_set_bidirectional_transmit_only_mode(SPI2);
    spi_enable_software_slave_management(SPI2);
    spi_set_nss_high(SPI2);
    spi_enable(SPI2);
}

spi_guard_status_t phase_shifter_write(uint16_t command, uint32_t timeout_millis)
{
    if (command > PHASE_COMMAND_MAX) {
        return SPI_GUARD_INVALID_ARGUMENT;
    }

    spi_guard_status_t status = spi_guard_wait_txe(SPI2, timeout_millis);
    if (status != SPI_GUARD_OK) {
        return status;
    }

    gpio_clear(SPI2_PS_LE_PORT, SPI2_PS_LE_PIN);
    SPI_DR(SPI2) = command;
    __asm__ volatile("dsb" ::: "memory");

    status = spi_guard_wait_complete(SPI2, timeout_millis);
    if (status != SPI_GUARD_OK) {
        return status;
    }

    control_line_timing_margin();
    gpio_set(SPI2_PS_LE_PORT, SPI2_PS_LE_PIN);
    control_line_timing_margin();
    return SPI_GUARD_OK;
}
