#include "clock_control.h"

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <stddef.h>

#define CLOCK_STARTUP_ITERATION_LIMIT 1000000u

static bool wait_for_register(
    volatile const uint32_t *reg,
    uint32_t mask,
    uint32_t expected)
{
    for (uint32_t remaining = CLOCK_STARTUP_ITERATION_LIMIT; remaining > 0u; --remaining) {
        if ((*reg & mask) == expected) {
            return true;
        }
    }

    return false;
}

static void configure_48mhz_bus_and_flash(void)
{
    rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
    rcc_set_ppre(RCC_CFGR_PPRE_NODIV);
    flash_prefetch_enable();
    flash_set_ws(FLASH_ACR_LATENCY_024_048MHZ);
}

static void publish_48mhz_frequencies(void)
{
    rcc_apb1_frequency = FIRMWARE_CORE_CLOCK_HZ;
    rcc_ahb_frequency = FIRMWARE_CORE_CLOCK_HZ;
}

static bool try_hse_pll(void)
{
    rcc_osc_on(RCC_HSE);
    if (!wait_for_register(&RCC_CR, RCC_CR_HSERDY, RCC_CR_HSERDY)) {
        return false;
    }

    configure_48mhz_bus_and_flash();
    rcc_set_pll_multiplication_factor(RCC_CFGR_PLLMUL_MUL6);
    rcc_set_pll_source(RCC_CFGR_PLLSRC_HSE_CLK);
    rcc_set_pllxtpre(RCC_CFGR_PLLXTPRE_HSE_CLK);
    rcc_osc_on(RCC_PLL);

    if (!wait_for_register(&RCC_CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        return false;
    }

    rcc_set_sysclk_source(RCC_PLL);
    if (!wait_for_register(&RCC_CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) {
        return false;
    }

    RCC_CR |= RCC_CR_CSSON;
    publish_48mhz_frequencies();
    return true;
}

static bool try_hsi48(void)
{
    RCC_CR2 |= RCC_CR2_HSI48ON;
    if (!wait_for_register(&RCC_CR2, RCC_CR2_HSI48RDY, RCC_CR2_HSI48RDY)) {
        return false;
    }

    configure_48mhz_bus_and_flash();
    rcc_set_sysclk_source(RCC_HSI48);
    if (!wait_for_register(&RCC_CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI48)) {
        return false;
    }

    RCC_CR &= ~RCC_CR_CSSON;
    rcc_osc_off(RCC_PLL);
    rcc_osc_off(RCC_HSE);
    publish_48mhz_frequencies();
    return true;
}

bool clock_control_setup(firmware_clock_t *clock_source)
{
    if (clock_source == NULL) {
        return false;
    }

    if (try_hse_pll()) {
        *clock_source = FIRMWARE_CLOCK_HSE_PLL;
        return true;
    }

    if (try_hsi48()) {
        *clock_source = FIRMWARE_CLOCK_HSI48;
        return true;
    }

    *clock_source = FIRMWARE_CLOCK_UNKNOWN;
    return false;
}
