#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>

void watchdog_start(uint32_t timeout_millis);
void watchdog_service(void);

#endif
