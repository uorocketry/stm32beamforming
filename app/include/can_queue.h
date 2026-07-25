#ifndef CAN_QUEUE_H
#define CAN_QUEUE_H

#include "can_protocol.h"

#include <stdbool.h>
#include <stdint.h>

#define CAN_RX_QUEUE_CAPACITY 8u

typedef struct can_rx_queue {
    can_frame_t frames[CAN_RX_QUEUE_CAPACITY];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint16_t dropped;
} can_rx_queue_t;

void can_rx_queue_init(can_rx_queue_t *queue);
bool can_rx_queue_push_isr(can_rx_queue_t *queue, const can_frame_t *frame);
bool can_rx_queue_pop(can_rx_queue_t *queue, can_frame_t *frame);
uint8_t can_rx_queue_count(const can_rx_queue_t *queue);
uint16_t can_rx_queue_dropped(const can_rx_queue_t *queue);

#endif
