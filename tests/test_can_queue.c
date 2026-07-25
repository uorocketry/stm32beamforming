#include "can_queue.h"

#include <assert.h>
#include <stdio.h>

static can_frame_t numbered_frame(uint16_t number)
{
    can_frame_t frame = {0};
    frame.id = number;
    frame.extended = true;
    frame.length = 2u;
    frame.data[0] = (uint8_t)(number & 0xffu);
    frame.data[1] = (uint8_t)(number >> 8u);
    return frame;
}

static void test_fifo_order(void)
{
    can_rx_queue_t queue;
    can_rx_queue_init(&queue);

    for (uint16_t value = 0u; value < CAN_RX_QUEUE_CAPACITY; ++value) {
        const can_frame_t frame = numbered_frame(value);
        assert(can_rx_queue_push_isr(&queue, &frame));
    }
    assert(can_rx_queue_count(&queue) == CAN_RX_QUEUE_CAPACITY);

    for (uint16_t value = 0u; value < CAN_RX_QUEUE_CAPACITY; ++value) {
        can_frame_t frame = {0};
        assert(can_rx_queue_pop(&queue, &frame));
        assert(frame.id == value);
        assert(frame.data[0] == (uint8_t)value);
    }
    assert(can_rx_queue_count(&queue) == 0u);
}

static void test_full_queue_drops_new_frame(void)
{
    can_rx_queue_t queue;
    can_rx_queue_init(&queue);

    for (uint16_t value = 0u; value < CAN_RX_QUEUE_CAPACITY; ++value) {
        const can_frame_t frame = numbered_frame(value);
        assert(can_rx_queue_push_isr(&queue, &frame));
    }

    const can_frame_t overflow = numbered_frame(99u);
    assert(!can_rx_queue_push_isr(&queue, &overflow));
    assert(can_rx_queue_dropped(&queue) == 1u);

    can_frame_t first = {0};
    assert(can_rx_queue_pop(&queue, &first));
    assert(first.id == 0u);
}

static void test_wraparound(void)
{
    can_rx_queue_t queue;
    can_rx_queue_init(&queue);

    for (uint16_t value = 0u; value < 300u; ++value) {
        const can_frame_t input = numbered_frame(value);
        assert(can_rx_queue_push_isr(&queue, &input));

        can_frame_t output = {0};
        assert(can_rx_queue_pop(&queue, &output));
        assert(output.id == value);
    }

    assert(can_rx_queue_count(&queue) == 0u);
    assert(can_rx_queue_dropped(&queue) == 0u);
}

static void test_invalid_arguments(void)
{
    can_rx_queue_t queue;
    can_rx_queue_init(&queue);
    const can_frame_t frame = numbered_frame(1u);

    assert(!can_rx_queue_push_isr(NULL, &frame));
    assert(!can_rx_queue_push_isr(&queue, NULL));
    assert(!can_rx_queue_pop(NULL, NULL));
    assert(!can_rx_queue_pop(&queue, NULL));
    assert(can_rx_queue_count(NULL) == 0u);
    assert(can_rx_queue_dropped(NULL) == 0u);
}

int main(void)
{
    test_fifo_order();
    test_full_queue_drops_new_frame();
    test_wraparound();
    test_invalid_arguments();
    puts("CAN queue tests passed");
    return 0;
}
