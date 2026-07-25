#include "faults.h"

#include "diagnostics.h"

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>

void firmware_fail(firmware_fault_t fault)
{
    diagnostics_set_fault(fault);
    scb_reset_system();

    while (1) {
        __asm__ volatile("nop");
    }
}

void hard_fault_handler(void)
{
    firmware_fail(FIRMWARE_FAULT_HARD_FAULT);
}

void nmi_handler(void)
{
    if (rcc_css_int_flag() != 0) {
        rcc_css_int_clear();
        firmware_fail(FIRMWARE_FAULT_CLOCK_SECURITY);
    }

    firmware_fail(FIRMWARE_FAULT_UNEXPECTED_INTERRUPT);
}

void unexpected_interrupt_handler(void)
{
    firmware_fail(FIRMWARE_FAULT_UNEXPECTED_INTERRUPT);
}

#define UNEXPECTED_ISR_ALIAS(name) \
    void name(void) __attribute__((alias("unexpected_interrupt_handler"), noreturn))

UNEXPECTED_ISR_ALIAS(wwdg_isr);
UNEXPECTED_ISR_ALIAS(pvd_isr);
UNEXPECTED_ISR_ALIAS(rtc_isr);
UNEXPECTED_ISR_ALIAS(flash_isr);
UNEXPECTED_ISR_ALIAS(rcc_isr);
UNEXPECTED_ISR_ALIAS(exti0_1_isr);
UNEXPECTED_ISR_ALIAS(exti2_3_isr);
UNEXPECTED_ISR_ALIAS(exti4_15_isr);
UNEXPECTED_ISR_ALIAS(tsc_isr);
UNEXPECTED_ISR_ALIAS(dma1_channel1_isr);
UNEXPECTED_ISR_ALIAS(dma1_channel2_3_dma2_channel1_2_isr);
UNEXPECTED_ISR_ALIAS(dma1_channel4_7_dma2_channel3_5_isr);
UNEXPECTED_ISR_ALIAS(adc_comp_isr);
UNEXPECTED_ISR_ALIAS(tim1_brk_up_trg_com_isr);
UNEXPECTED_ISR_ALIAS(tim1_cc_isr);
UNEXPECTED_ISR_ALIAS(tim2_isr);
UNEXPECTED_ISR_ALIAS(tim3_isr);
UNEXPECTED_ISR_ALIAS(tim6_dac_isr);
UNEXPECTED_ISR_ALIAS(tim7_isr);
UNEXPECTED_ISR_ALIAS(tim14_isr);
UNEXPECTED_ISR_ALIAS(tim15_isr);
UNEXPECTED_ISR_ALIAS(tim16_isr);
UNEXPECTED_ISR_ALIAS(tim17_isr);
UNEXPECTED_ISR_ALIAS(i2c1_isr);
UNEXPECTED_ISR_ALIAS(i2c2_isr);
UNEXPECTED_ISR_ALIAS(spi1_isr);
UNEXPECTED_ISR_ALIAS(spi2_isr);
UNEXPECTED_ISR_ALIAS(usart1_isr);
UNEXPECTED_ISR_ALIAS(usart2_isr);
UNEXPECTED_ISR_ALIAS(usart3_4_isr);
UNEXPECTED_ISR_ALIAS(usb_isr);
