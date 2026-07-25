#ifndef CAN_FILTER_H
#define CAN_FILTER_H

#include <stdbool.h>
#include <stdint.h>

bool can_filter_for_destination(
    uint8_t destination,
    uint32_t *identifier_register,
    uint32_t *mask_register);

#endif
