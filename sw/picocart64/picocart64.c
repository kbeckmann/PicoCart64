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
#include "picocart64_pins.h"
#include "picocart64.h"
#include "ringbuf.h"
#include "sram.h"
#include "utils.h"

// The rom to load in normal .z64, big endian, format
#include "rom.h"
static const uint16_t *rom_file_16 = (uint16_t *) rom_file;

RINGBUF_CREATE(ringbuf, 64);

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

static inline uint32_t n64_pi_get_value(PIO pio)
{
    uint32_t value = pio_sm_get_blocking(pio, 0);

    // Disable to get some more performance. Enable for debugging.
    // Without ringbuf, ROM access takes 160-180ns. With, 240-260ns.
#if 0
    ringbuf_add(ringbuf, value);
#endif

    return value;
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

    uint32_t last_addr;
    uint32_t addr;
    uint32_t next_word;

    // Read addr manually before the loop
    addr = n64_pi_get_value(pio);

    while (1) {
        // addr must not be a WRITE or READ request here,
        // it should contain a 16-bit aligned address.
        assert((addr != 0) && ((addr & 1) == 0));

        // We got a start address
        last_addr = addr;

        // Handle access based on memory region
        // Note that the if-cases are ordered in priority from
        // most timing critical to least.
        if (last_addr == 0x10000000) {
            // Configure bus to run slowly.
            // This is better patched in the rom, so we won't need a branch here.
            // But let's keep it here so it's easy to import roms.

            // 0x8037FF40 in big-endian
            next_word = 0x8037;
            addr = n64_pi_get_value(pio);

            // Assume addr == 0, i.e. READ request
            pio_sm_put(pio, 0, next_word);
            last_addr += 2;

            // Patch bus speed here if needed (e.g. if not overclocking)
            // next_word = 0xFF40;
            // next_word = 0x2040;

            // Official SDK standard speed
            next_word = 0x1240;
            addr = n64_pi_get_value(pio);

            // Assume addr == 0, i.e. push 16 bits of data
            pio_sm_put(pio, 0, next_word);
            last_addr += 2;

            // Pre-fetch
            next_word = rom_file_16[(last_addr & 0xFFFFFF) >> 1];

            // ROM patching done
            addr = n64_pi_get_value(pio);
            if (addr == 0) {
                // I apologise for the use of goto, but it seemed like a fast way
                // to enter the next state immediately.
                goto handle_d1a2_read;
            } else {
                continue;
            }
        } else if (last_addr >= 0x08000000 && last_addr <= 0x0FFFFFFF) {
            // Domain 2, Address 2 Cartridge SRAM
            do {
                // Pre-fetch from the address
                next_word = sram[resolve_sram_address(last_addr) >> 1];

                // Read command/address
                addr = n64_pi_get_value(pio);

                if (addr & 0x00000001) {
                    // We got a WRITE
                    // 0bxxxxxxxx_xxxxxxxx_11111111_11111111
                    sram[resolve_sram_address(last_addr) >> 1] = addr >> 16;
                    last_addr += 2;
                } else if (addr == 0) {
                    // READ
                    pio_sm_put(pio, 0, next_word);
                    last_addr += 2;
                    next_word = sram[resolve_sram_address(last_addr) >> 1];
                } else {
                    // New address
                    break;
                }
            } while(1);
        } else if (last_addr >= 0x10000000 && last_addr <= 0x1FBFFFFF) {
            // Domain 1, Address 2 Cartridge ROM
            do {
                // Pre-fetch from the address
                next_word = rom_file_16[(last_addr & 0xFFFFFF) >> 1];

                // Read command/address
                addr = n64_pi_get_value(pio);

                if (addr == 0) {
                    // READ
handle_d1a2_read:
                    pio_sm_put(pio, 0, swap8(next_word));
                    last_addr += 2;
                } else if (addr & 0x00000001) {
                    // WRITE
                    // Ignore data since we're asked to write to the ROM.
                    last_addr += 2;
                } else {
                    // New address
                    break;
                }
            } while(1);
        } else if (last_addr >= 0x05000000 && last_addr <= 0x05FFFFFF) {
            // Domain 2, Address 1 N64DD control registers
            do {
                // We don't support this yet, but we have to consume another value
                next_word = 0;

                // Read command/address
                addr = n64_pi_get_value(pio);

                if (addr == 0) {
                    // READ
                    pio_sm_put(pio, 0, next_word);
                    last_addr += 2;
                } else if (addr & 0x00000001) {
                    // WRITE
                    // Ignore
                    last_addr += 2;
                } else {
                    // New address
                    break;
                }
            } while(1);
        } else if (last_addr >= 0x06000000 && last_addr <= 0x07FFFFFF) {
            // Domain 2, Address 1 N64DD control registers
            do {
                // We don't support this yet, but we have to consume another value
                next_word = 0;

                // Read command/address
                addr = n64_pi_get_value(pio);

                if (addr == 0) {
                    // READ
                    pio_sm_put(pio, 0, next_word);
                    last_addr += 2;
                } else if (addr & 0x00000001) {
                    // WRITE
                    // Ignore
                    last_addr += 2;
                } else {
                    // New address
                    break;
                }
            } while(1);
        } else {
            do {
                // We don't support this memory area yet, but we have to consume another value
                next_word = 0;

                // Read command/address
                addr = n64_pi_get_value(pio);

                if (addr == 0) {
                    // READ
                    pio_sm_put(pio, 0, swap8(next_word));
                    last_addr += 2;
                } else if (addr & 0x00000001) {
                    // WRITE
                    // Ignore
                    last_addr += 2;
                } else {
                    // New address
                    break;
                }
            } while(1);
        }
    }

    return 0;
}
