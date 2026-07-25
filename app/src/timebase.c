#include "timebase.h"

#include <libopencm3/cm3/systick.h>

static volatile uint32_t system_millis;

void timebase_setup(uint32_t core_clock_hz)
{
    system_millis = 0u;
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload((core_clock_hz / 1000u) - 1u);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();
}

uint32_t timebase_millis(void)
{
    return system_millis;
}

bool timebase_elapsed(uint32_t start_millis, uint32_t timeout_millis)
{
    return (uint32_t)(timebase_millis() - start_millis) >= timeout_millis;
}

void sys_tick_handler(void)
{
    ++system_millis;
}
