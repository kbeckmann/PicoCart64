/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"

#include "cic.h"
#include "picocart64_pins.h"
#include "picocart64.h"
#include "utils.h"
#include "n64_pi.h"

#define PICO_LA1    (26)
#define PICO_LA2    (27)

#define UART_TX_PIN (28)
#define UART_RX_PIN (29) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

static bool core1_running;

static void core0_sio_irq()
{
    uint32_t core0_rx_val = 0;

    while (multicore_fifo_rvalid())
        core0_rx_val = multicore_fifo_pop_blocking();

    if (core0_rx_val == CORE1_FLAG_BOOT) {
        core1_running = true;
    }

    multicore_fifo_clear_irq();
}


/*

Profiling results:

Time between ~N64_READ and bit output on AD0

133 MHz old code:
    ROM:  1st _980_ ns, 2nd 500 ns
    SRAM: 1st  500  ns, 2nd 510 ns

133 MHz new code:
    ROM:  1st _300_ ns, 2nd 280 ns
    SRAM: 1st  320  ns, 2nd 320 ns

266 MHz new code:
    ROM:  1st  180 ns, 2nd 180 ns (sometimes down to 160, but only worst case matters)
    SRAM: 1st  160 ns, 2nd 160 ns

*/

int main(void)
{
    // Overclock!
    // The external flash should be rated to 133MHz,
    // but since it's used with a 2x clock divider,
    // 266 MHz is safe in this regard.

    // set_sys_clock_khz(PLL_SYS_KHZ, true);
    set_sys_clock_khz(266000, true); // Required for SRAM @ 200ns

    // stdio_init_all();

    for (int i = 0; i <= 27; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }

    gpio_init(N64_CIC_DCLK);
    gpio_init(N64_CIC_DIO);
    gpio_init(N64_COLD_RESET);

    gpio_pull_up(N64_CIC_DIO);

    // Init UART on pin 28/29
    stdio_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    printf("PicoCart64 Booting!\r\n");

    // Set up IRQ to let core1 interrupt core0
    irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_sio_irq);
    irq_set_enabled(SIO_IRQ_PROC0, true);

    // Launch the N64 PI implementation in the second core
    // Note! You have to power reset the pico after flashing it with a jlink,
    //       otherwise multicore doesn't work properly.
    //       Alternatively, attach gdb to openocd, run `mon reset halt`, `c`.
    //       It seems this works around the issue as well.
    multicore_launch_core1(n64_pi_run);

    // Wait for core1 to finish booting
    while (!core1_running) {
        tight_loop_contents();
    }

    // Launch the CIC emulator on the primary core
    // TODO: Integrate FreeRTOS or similar
    cic_main();

    return 0;
}
