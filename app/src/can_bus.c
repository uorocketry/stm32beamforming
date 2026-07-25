#include "can_bus.h"

#include "can_filter.h"
#include "can_queue.h"
#include "clock_control.h"
#include "firmware_config.h"

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define CAN_RX_PORT GPIOA
#define CAN_RX_PIN GPIO11
#define CAN_TX_PORT GPIOA
#define CAN_TX_PIN GPIO12
#define CAN_GPIO_ALTERNATE_FUNCTION GPIO_AF4
#define CAN_FILTER_SELF_BANK 0u
#define CAN_FILTER_BROADCAST_BANK 1u
#define CAN_RECEIVE_FIFO_INDEX 0u
#define CAN_BITRATE_PRESCALER \
    (FIRMWARE_CORE_CLOCK_HZ / (CAN_BUS_BITRATE * CAN_TIME_QUANTA_PER_BIT))

_Static_assert(
    (FIRMWARE_CORE_CLOCK_HZ % (CAN_BUS_BITRATE * CAN_TIME_QUANTA_PER_BIT)) == 0u,
    "CAN bitrate must divide the firmware core clock exactly");
_Static_assert(CAN_BITRATE_PRESCALER == 6u, "unexpected 500 kbit/s CAN prescaler");

static can_rx_queue_t receive_queue;
static volatile uint16_t hardware_rx_dropped;
static uint16_t transmit_dropped;
static bool initialized;

static uint16_t saturating_add_u16(uint16_t left, uint16_t right)
{
    return (right > (uint16_t)(UINT16_MAX - left))
        ? UINT16_MAX
        : (uint16_t)(left + right);
}

static void increment_saturating(volatile uint16_t *counter)
{
    if (*counter != UINT16_MAX) {
        ++(*counter);
    }
}

static bool configure_destination_filter(uint32_t bank, uint8_t destination)
{
    uint32_t identifier = 0u;
    uint32_t mask = 0u;
    if (!can_filter_for_destination(destination, &identifier, &mask)) {
        return false;
    }

    can_filter_id_mask_32bit_init(
        bank,
        identifier,
        mask,
        CAN_RECEIVE_FIFO_INDEX,
        true);
    return true;
}

bool can_bus_setup(uint8_t self_node)
{
    if (self_node < CAN_NODE_MIN || self_node > CAN_NODE_MAX) {
        return false;
    }

    initialized = false;
    hardware_rx_dropped = 0u;
    transmit_dropped = 0u;
    can_rx_queue_init(&receive_queue);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_CAN1);

    gpio_mode_setup(
        CAN_RX_PORT,
        GPIO_MODE_AF,
        GPIO_PUPD_PULLUP,
        CAN_RX_PIN | CAN_TX_PIN);
    gpio_set_af(
        CAN_RX_PORT,
        CAN_GPIO_ALTERNATE_FUNCTION,
        CAN_RX_PIN | CAN_TX_PIN);
    gpio_set_output_options(
        CAN_TX_PORT,
        GPIO_OTYPE_PP,
        GPIO_OSPEED_HIGH,
        CAN_TX_PIN);

    can_reset(CAN1);
    if (can_init(
            CAN1,
            false,
            true,
            false,
            false,
            true,
            false,
            CAN_BTR_SJW_1TQ,
            CAN_BTR_TS1_13TQ,
            CAN_BTR_TS2_2TQ,
            CAN_BITRATE_PRESCALER,
            false,
            false) != 0) {
        return false;
    }

    if (!configure_destination_filter(CAN_FILTER_SELF_BANK, self_node)
        || !configure_destination_filter(CAN_FILTER_BROADCAST_BANK, CAN_NODE_BROADCAST)) {
        return false;
    }

    can_enable_irq(CAN1, CAN_IER_FMPIE0 | CAN_IER_FOVIE0);
    nvic_clear_pending_irq(NVIC_CEC_CAN_IRQ);
    nvic_enable_irq(NVIC_CEC_CAN_IRQ);
    initialized = true;
    return true;
}

bool can_bus_receive(can_frame_t *frame)
{
    if (!initialized || frame == NULL) {
        return false;
    }

    return can_rx_queue_pop(&receive_queue, frame);
}

bool can_bus_send(const can_frame_t *frame)
{
    if (!initialized
        || frame == NULL
        || !frame->extended
        || frame->remote
        || frame->id > CAN_EXTENDED_ID_MAX
        || frame->length > CAN_MAX_DATA_LENGTH) {
        return false;
    }

    uint8_t data[CAN_MAX_DATA_LENGTH] = {0};
    memcpy(data, frame->data, frame->length);
    if (can_transmit(
            CAN1,
            frame->id,
            true,
            false,
            frame->length,
            data) < 0) {
        if (transmit_dropped != UINT16_MAX) {
            ++transmit_dropped;
        }
        return false;
    }

    return true;
}

uint16_t can_bus_rx_dropped(void)
{
    return saturating_add_u16(
        hardware_rx_dropped,
        can_rx_queue_dropped(&receive_queue));
}

uint16_t can_bus_tx_dropped(void)
{
    return transmit_dropped;
}

bool can_bus_is_bus_off(void)
{
    return initialized && ((CAN_ESR(CAN1) & CAN_ESR_BOFF) != 0u);
}

void cec_can_isr(void)
{
    if ((CAN_RF0R(CAN1) & CAN_RF0R_FOVR0) != 0u) {
        increment_saturating(&hardware_rx_dropped);
        CAN_RF0R(CAN1) |= CAN_RF0R_FOVR0;
    }

    while (can_fifo_pending(CAN1, CAN_RECEIVE_FIFO_INDEX) > 0u) {
        can_frame_t frame = {0};
        uint8_t filter_match_index = 0u;

        can_receive(
            CAN1,
            CAN_RECEIVE_FIFO_INDEX,
            true,
            &frame.id,
            &frame.extended,
            &frame.remote,
            &filter_match_index,
            &frame.length,
            frame.data,
            NULL);

        (void)filter_match_index;
        (void)can_rx_queue_push_isr(&receive_queue, &frame);
    }
}
