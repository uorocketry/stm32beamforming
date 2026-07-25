#include "watchdog.h"

#include <libopencm3/stm32/iwdg.h>

void watchdog_start(uint32_t timeout_millis)
{
    iwdg_set_period_ms(timeout_millis);
}

void watchdog_service(void)
{
    iwdg_reset();
}
