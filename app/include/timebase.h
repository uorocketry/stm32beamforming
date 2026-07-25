#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

void timebase_setup(uint32_t core_clock_hz);
uint32_t timebase_millis(void);
bool timebase_elapsed(uint32_t start_millis, uint32_t timeout_millis);
void sys_tick_handler(void);

#endif
