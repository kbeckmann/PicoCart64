/**
 * Copyright (c) 2022 Konrad Beckmann
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"

#include "cic_test.pio.h"

#include "cic.h"
#include "n64.h"

// The rom to load in normal .z64, big endian, format
#include "rom.h"
const uint32_t *rom_file_32 = (uint32_t *) rom_file;

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define PICO_LA1    (26)
#define PICO_LA2    (27)

#define UART_TX_PIN (28)
#define UART_RX_PIN (29) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200


static inline uint32_t swap16(uint32_t value)
{
    // 0x11223344 => 0x33441122
    return (value << 16) | (value >> 16);
}


int main(void)
{
    // Overclock!
    // set_sys_clock_khz(250000, true); // 171us

    stdio_init_all();

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

    // Init PIO before starting the second core
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &cart_program);
    cart_program_init(pio, 0, offset);
    pio_sm_set_enabled(pio, 0, true);

    // Launch the CIC emulator in the second core
    // Note! You have to power reset the pico after flashing it with a jlink,
    //       otherwise multicore doesn't work properly.
    //       Alternatively, attach gdb to openocd, run `mon reset halt`, `c`.
    //       It seems this works around the issue as well.
    multicore_launch_core1(cic_main);

    // Wait for reset to be released
    while (gpio_get(N64_COLD_RESET) == 0) {
        tight_loop_contents();
    }

    uint32_t n64_addr = 0;
    uint32_t n64_addr_h = 0;
    uint32_t n64_addr_l = 0;

    uint32_t last_addr = 0;
    uint32_t get_msb = 0;

    while (1) {
        uint32_t addr = swap16(pio_sm_get_blocking(pio, 0));

        if (addr != 0) {
            // We got a start address
            last_addr = addr;
            get_msb = 1;
            continue;
        }

        // We got a "Give me next 16 bits" command
        static uint32_t word;
        if (get_msb) {
            if (last_addr == 0x10000000) {
                // Configure bus to run slowly.
                // This is better patched in the rom, so we won't need a branch here.
                // But let's keep it here so it's easy to import roms easily.
                // 0x8037FF40 in big-endian
                word = 0x40FF3780;
            } else {
                word = rom_file_32[(last_addr & 0xFFFFFF) >> 2];
            }
            pio_sm_put_blocking(pio, 0, ((word << 8) & 0xFF00)  | ((word >> 8) & 0xFF));
        } else {
            pio_sm_put_blocking(pio, 0, ((word >> 8) & 0xFF00)  | (word >> 24));
            last_addr += 4;
        }

        get_msb = !get_msb;
    }

    return 0;
}
