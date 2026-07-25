#ifndef CAN_BUS_H
#define CAN_BUS_H

#include "can_protocol.h"

#include <stdbool.h>
#include <stdint.h>

bool can_bus_setup(uint8_t self_node);
bool can_bus_receive(can_frame_t *frame);
bool can_bus_send(const can_frame_t *frame);
uint16_t can_bus_rx_dropped(void);
uint16_t can_bus_tx_dropped(void);
bool can_bus_is_bus_off(void);

#endif
