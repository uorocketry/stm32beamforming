#include "can_filter.h"
#include "can_protocol.h"

#include <stddef.h>

#define BXCAN_EXTENDED_ID_SHIFT 3u
#define BXCAN_IDE_BIT (1u << 2)
#define BXCAN_RTR_BIT (1u << 1)

bool can_filter_for_destination(
    uint8_t destination,
    uint32_t *identifier_register,
    uint32_t *mask_register)
{
    if (destination > CAN_NODE_BROADCAST
        || identifier_register == NULL
        || mask_register == NULL) {
        return false;
    }

    const uint32_t destination_bits =
        (uint32_t)destination << CAN_ID_DESTINATION_SHIFT;
    const uint32_t destination_mask =
        (uint32_t)CAN_ID_NODE_MASK << CAN_ID_DESTINATION_SHIFT;

    *identifier_register = (destination_bits << BXCAN_EXTENDED_ID_SHIFT) | BXCAN_IDE_BIT;
    *mask_register = (destination_mask << BXCAN_EXTENDED_ID_SHIFT)
        | BXCAN_IDE_BIT
        | BXCAN_RTR_BIT;
    return true;
}
