#ifndef CLOCK_CONTROL_H
#define CLOCK_CONTROL_H

#include "diagnostic_record.h"

#include <stdbool.h>

#define FIRMWARE_CORE_CLOCK_HZ 48000000u

bool clock_control_setup(firmware_clock_t *clock_source);

#endif
