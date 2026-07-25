#include "can_queue.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

_Static_assert(
    (CAN_RX_QUEUE_CAPACITY & (CAN_RX_QUEUE_CAPACITY - 1u)) == 0u,
    "CAN_RX_QUEUE_CAPACITY must be a power of two");
_Static_assert(CAN_RX_QUEUE_CAPACITY < 128u, "queue capacity must fit wrap-safe distance");

#define CAN_QUEUE_INDEX_MASK (CAN_RX_QUEUE_CAPACITY - 1u)
#define CAN_QUEUE_COMPILER_BARRIER() __asm__ volatile("" ::: "memory")

void can_rx_queue_init(can_rx_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    memset(queue, 0, sizeof(*queue));
}

uint8_t can_rx_queue_count(const can_rx_queue_t *queue)
{
    if (queue == NULL) {
        return 0u;
    }

    return (uint8_t)(queue->head - queue->tail);
}

bool can_rx_queue_push_isr(can_rx_queue_t *queue, const can_frame_t *frame)
{
    if (queue == NULL || frame == NULL) {
        return false;
    }

    const uint8_t head = queue->head;
    if ((uint8_t)(head - queue->tail) >= CAN_RX_QUEUE_CAPACITY) {
        if (queue->dropped != UINT16_MAX) {
            ++queue->dropped;
        }
        return false;
    }

    queue->frames[head & CAN_QUEUE_INDEX_MASK] = *frame;
    CAN_QUEUE_COMPILER_BARRIER();
    queue->head = (uint8_t)(head + 1u);
    return true;
}

bool can_rx_queue_pop(can_rx_queue_t *queue, can_frame_t *frame)
{
    if (queue == NULL || frame == NULL) {
        return false;
    }

    const uint8_t tail = queue->tail;
    if (tail == queue->head) {
        return false;
    }

    *frame = queue->frames[tail & CAN_QUEUE_INDEX_MASK];
    CAN_QUEUE_COMPILER_BARRIER();
    queue->tail = (uint8_t)(tail + 1u);
    return true;
}

uint16_t can_rx_queue_dropped(const can_rx_queue_t *queue)
{
    return (queue == NULL) ? 0u : queue->dropped;
}
