#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <include/PhaseShifter.h>
#include <include/Vga.h>
#include <stdint.h>
#include <libopencm3/stm32/timer.h>
#include <stdbool.h>
#include <include/ProcessHostCmds.h>
#include <include/beamformerCan.h>




int main(void)
{
    rcc_clock_setup_in_hsi_out_48mhz(); 
    pe448spisetup(); //phase shifter spi setup connected to this device
    f0480spisetup();//vga spi setup
                                    
    spi_enable(SPI2);
    spi_enable(SPI1);

    // No UART on RX4: the RX1 RS485 pins (PB6, PB7, PC11, PC12) are STBY,
    // unconnected and power setpoints on this board. Do NOT call init_uart_rx();
    // it would hand PB6 (STBY) to USART1, which idles HIGH and puts the VGAs in standby.

    canSetUpClocks(); 
    canSetUpAcceptanceFilters(CAN_FILTER_MCU1_MASK); //if this is self 



    while (1)
    {
        // Can message received and passed acceptance filters
        if (can_data_pending) {
            can_data_pending = false; //clear
            process_host_cmd_can(can_data_rx, can_id_rx);

        }
        

    }
    return 0;
}