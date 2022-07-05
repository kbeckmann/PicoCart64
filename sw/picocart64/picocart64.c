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
#include "hardware/irq.h"

#include "n64_pi.pio.h"

#include "cic.h"
#include "picocart64.h"
#include "picocart64_pins.h"
#include "sram.h"

// The rom to load in normal .z64, big endian, format
#include "rom.h"
static const uint16_t *rom_file_16 = (uint16_t *) rom_file;

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

static inline uint32_t swap8(uint16_t value)
{
    // 0x1122 => 0x2211
    return (value << 8) | (value >> 8);
}

static inline uint32_t resolve_sram_address(uint32_t address)
{
    uint32_t bank = (address >> 18) & 0x3;
    uint32_t resolved_address;

    if (bank) {
        resolved_address = address & (SRAM_256KBIT_SIZE - 1);
        resolved_address |= bank << 15;
    } else {
        resolved_address = address & (sizeof(sram) - 1);
    }

    return resolved_address;
}

static void core0_sio_irq()
{
    uint32_t core0_rx_val = 0;

    while (multicore_fifo_rvalid())
        core0_rx_val = multicore_fifo_pop_blocking();

    if (core0_rx_val == CORE1_FLAG_N64_CR) {
        // CORE1_FLAG_N64_CR from core1 means we should save sram.
        // Do it from irq context for now (bad idea!).
        // TODO: Maybe unblock the pio_sm_get_blocking call
        //       by resetting the state machine?
        sram_save_to_flash();
    }

    multicore_fifo_clear_irq();
}


/*

Profiling results:

Time between ~N64_READ and bit output on AD0

With constant data fetched from C-code (no memory access)
--------------------------------------
133 MHz: 240 ns
150 MHz: 230 ns
200 MHz: 230 ns
250 MHz: 190 ns


With uncached data from external flash
--------------------------------------
133 MHz: 780 ns
150 MHz: 640 ns
200 MHz: 480 ns
250 MHz: 390 ns



*/

int main(void)
{
    // Overclock!
    // Note that the Pico's external flash is rated to 133MHz,
    // not sure if the flash speed is based on this clock.

    // set_sys_clock_khz(PLL_SYS_KHZ, true);
    // set_sys_clock_khz(150000, true); // Does not work
    // set_sys_clock_khz(200000, true); // Does not work
    // set_sys_clock_khz(250000, true); // Does not work
    // set_sys_clock_khz(300000, true); // Doesn't even boot
    // set_sys_clock_khz(400000, true); // Doesn't even boot

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

    // Init PIO before starting the second core
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &n64_pi_program);
    n64_pi_program_init(pio, 0, offset);
    pio_sm_set_enabled(pio, 0, true);

    // Set up IRQ to let core1 interrupt core0
    irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_sio_irq);
    irq_set_enabled(SIO_IRQ_PROC0, true);

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

        if (addr & 0x00000001) {
            // We got a WRITE
            // 0bxxxxxxxx_xxxxxxxx_11111111_11111111
            if (last_addr >= 0x08000000 && last_addr <= 0x0FFFFFFF) {
                sram[resolve_sram_address(last_addr) >> 1] = addr >> 16;
            }
            last_addr += 2;
            continue;
        }

        if (addr != 0) {
            // We got a start address
            last_addr = addr;
            get_msb = 1;
            continue;
        }

        // We got a "Give me next 16 bits" command
        uint32_t word;
        if (last_addr == 0x10000000) {
            // Configure bus to run slowly.
            // This is better patched in the rom, so we won't need a branch here.
            // But let's keep it here so it's easy to import roms easily.
            // 0x8037FF40 in big-endian
            word = 0x8037;
            pio_sm_put_blocking(pio, 0, word);
        } else if (last_addr == 0x10000002) {
            // Configure bus to run slowly.
            // This is better patched in the rom, so we won't need a branch here.
            // But let's keep it here so it's easy to import roms easily.
            // 0x8037FF40 in big-endian
            word = 0xFF40;
            pio_sm_put_blocking(pio, 0, word);
        } else if (last_addr >= 0x08000000 && last_addr <= 0x0FFFFFFF) {
            // Domain 2, Address 2 Cartridge SRAM
            word = sram[resolve_sram_address(last_addr) >> 1];
            pio_sm_put_blocking(pio, 0, word);
        } else if (last_addr >= 0x10000000 && last_addr <= 0x1FBFFFFF) {
            // Domain 1, Address 2 Cartridge ROM
            word = rom_file_16[(last_addr & 0xFFFFFF) >> 1];
            pio_sm_put_blocking(pio, 0, swap8(word));
        }

        last_addr += 2;
    }

    return 0;
}
