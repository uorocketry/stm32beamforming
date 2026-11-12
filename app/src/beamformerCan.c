//SN65HVD230 is the CAN transciever we are connecting to the cpu in RX4 configuration
//https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf?ts=1778578757681&ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252FSN65HVD230

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <include/beamformerCan.h>
#include <include/PhaseShifter.h>
#include <include/Vga.h>

volatile bool can_data_pending = false; 
uint8_t can_data_rx[8];
volatile uint32_t can_id_rx;


typedef struct Can_Message_rx_global { 
    uint8_t can_data_rx[8];
    uint32_t id; //later mask, use first 3 bits to determine command ID 
    bool ext;
    bool rtr;
    uint8_t fmi;
    uint8_t length;
    uint8_t data[8];

} Can_Message_rx_global_t; 

static Can_Message_rx_global_t unprocessedCanCmd; 



void canSetUpClocks(void) 
{ 
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_CAN1);
    
    //enable CAN interrupts 
    // PA11 = CAN RX
    // PA12 = CAN TX
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, CAN1_RX_PIN | CAN1_TX_PIN); 
    //pullup so that we idle high. CAN bus idle state is recessive (logic 1), SOF is dominant (logic 0)
    gpio_set_af(GPIOA, GPIO_AF4, CAN1_TX_PIN | CAN1_RX_PIN);
    can_reset(CAN1);
    can_init(
        CAN1,
        false,              // time triggered communication mode
        true,               // automatic bus-off management
        false,              // automatic wakeup mode
        false,              // no automatic retransmission
        false,              // receive FIFO locked mode
        false,              // transmit FIFO priority
        CAN_BTR_SJW_1TQ,
        CAN_BTR_TS1_13TQ,
        CAN_BTR_TS2_2TQ,
        6,                  // 48 MHz / (6 * 16 tq) = 500 kbit/s
        false,
        false
    );
} 

void canSetUpAcceptanceFilters(CanTargetMcuMask_e canTargetMcuMaskSelf)
{
    /*
    How the filter works (32-bit id/mask mode):
    - mask: which bits of the incoming ID the hardware checks (1 = check, 0 = ignore)
    - id: what those checked bits must be for the message to be accepted
    We only check one bit, and want it to be 1, so the same value is used for both.
    */

    /*
    The filter registers don't store the ID starting at bit 0. Their layout is:
        bits 31-3 = 29-bit extended ID, bit 2 = IDE, bit 1 = RTR, bit 0 = reserved
    can_filter_id_mask_32bit_init writes our values straight into those registers,
    so each ID bit has to be shifted left by 3 (CAN_RIxR_EXID_SHIFT) to line up.
    (can_receive does the opposite, shifting right by 3, so the ID we get in the ISR is unshifted.)
    Without the shift, MCU1's bit would land on the reserved bit 0 and never match.
    */

    const uint32_t self_bit_register_layout = (uint32_t)canTargetMcuMaskSelf << CAN_RIxR_EXID_SHIFT;
    
    //Bank 0: accept if this MCU's bit is set. 
    const uint32_t self_filter_id   = (uint32_t)self_bit_register_layout;
    const uint32_t self_filter_mask = (uint32_t)self_bit_register_layout;

    const uint32_t common_bit_register_layout = (uint32_t)CAN_FILTER_COMMON_MASK << CAN_RIxR_EXID_SHIFT;
    
    //Bank 1: accept if the common/all bit is set. 
    const uint32_t common_filter_id   = (uint32_t)common_bit_register_layout;
    const uint32_t common_filter_mask = (uint32_t)common_bit_register_layout;

    can_filter_id_mask_32bit_init(
        0,                  // filter bank 0 
        self_filter_id,    // filter ID
        self_filter_mask,  // mask
        CAN_FIFO0,         // route to the first receive FIFO
        true               // enable
    );

    can_filter_id_mask_32bit_init(
        1,                  // filter bank 1
        common_filter_id,  // filter ID
        common_filter_mask,// mask
        CAN_FIFO0,         // route to the first receive FIFO
        true               // enable
    );
    can_enable_irq(CAN1, CAN_IER_FMPIE0);
    nvic_enable_irq(NVIC_CEC_CAN_IRQ);
} 

void cec_can_isr(void)
{
    while (can_fifo_pending(CAN1, CAN_FIFO0) > 0) {
        //check acceptance fitlers success 
        can_receive(
            CAN1,
            CAN_FIFO0,
            true,      // release FIFO after reading
            &unprocessedCanCmd.id, 
            &unprocessedCanCmd.ext,
            &unprocessedCanCmd.rtr,
            &unprocessedCanCmd.fmi,
            &unprocessedCanCmd.length,
            unprocessedCanCmd.data,
            NULL
        ); 

        //first clear can_data_rx
        memset(can_data_rx, 0, sizeof(can_data_rx));
        memcpy(can_data_rx, unprocessedCanCmd.data, sizeof(unprocessedCanCmd.data));
        can_id_rx = unprocessedCanCmd.id; 
        
        can_data_pending = true; //flag for main loop 
    }
}

// //in main loop 
// if (can_data_pending == true) { 
        //can_data_pending = false;
        //processPendingCanCommand();

// } else if (uart_data_pending == true) { //second priority, check can first
        //uart_data_pending = false; 
        //process_host_cmd_uart(*buf, *extractedCmdLine, *processedHostCmd);
