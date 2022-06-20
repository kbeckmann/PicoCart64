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

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#include "hardware/structs/scb.h"

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

// Enable this define to use PIO. C-code statemachine otherwise.
#define PIO_MODE



typedef enum {
    STATE_INIT = 0,
    STATE_START,
    STATE_WAIT_ADDR_H,
    STATE_WAIT_ADDR_L,
    STATE_WAIT_READ_WRITE,
    STATE_WAIT_READ_H,
    STATE_WAIT_WRITE_H,
} cart_state_t;

const char* state_to_string[] = {
    "STATE_INIT",
    "STATE_START",
    "STATE_WAIT_ADDR_H",
    "STATE_WAIT_ADDR_L",
    "STATE_WAIT_READ_WRITE",
    "STATE_WAIT_READ_H",
    "STATE_WAIT_WRITE_H",
};

typedef enum {
    ENTRY_TYPE_STATE = 0,
    ENTRY_TYPE_ADDRESS,
} entry_type_t;

typedef struct {
    entry_type_t type;
    union {
        struct {
            uint16_t old_state;
            uint16_t new_state;
        } state;
        uint32_t address;
    };
} ringbuf_entry_t;


ringbuf_entry_t ringbuf[128];
uint32_t ringbuf_idx;
uint32_t ringbuf_ctr;

static inline uint32_t swap16(uint32_t value)
{
    // 0x11223344 => 0x33441122
    return (value << 16) | (value >> 16);
}

static void ringbuf_print_and_reset(void)
{
    for (int i = 0; i < ARRAY_SIZE(ringbuf); i++) {
        ringbuf_entry_t entry = ringbuf[i];
        int idx = ringbuf_ctr + i;

        switch (entry.type) {
        case ENTRY_TYPE_STATE:
            printf("%-4d: %-22s => %-22s\r\n", idx,
                state_to_string[entry.state.old_state],
                state_to_string[entry.state.new_state]);
            break;

        case ENTRY_TYPE_ADDRESS:
            printf("%-4d: Address = %08lX\r\n", idx, entry.address);
            break;
        
        default:
            printf("%-4d: %d = %08lX\r\n", idx, entry.type, entry.address);
        }
    }

    ringbuf_idx = 0;
    ringbuf_ctr += ARRAY_SIZE(ringbuf);
}


static void ringbuf_put(ringbuf_entry_t entry)
{
    ringbuf[ringbuf_idx++] = entry;

    if (ringbuf_idx < ARRAY_SIZE(ringbuf))
        return;

    ringbuf_print_and_reset();
}


static cart_state_t update_state(cart_state_t old_state, cart_state_t new_state)
{
    // ringbuf_put((ringbuf_entry_t) {
    //     .type = ENTRY_TYPE_STATE,
    //     .state.old_state = old_state,
    //     .state.new_state = new_state,
    // });

    return new_state;
}


static void cart_pin_forever(PIO pio, uint sm, uint offset) {
    cart_program_init(pio, sm, offset);
    pio_sm_set_enabled(pio, sm, true);
}

static void foo(void)
{
    // FIXME: Load-bearing print - remove this and it stops working
    printf("CIC Emulator core running!\r\n");

    cic_run();

    while (1) {
        cic_process();
    }
}


// uint32_t my_core1_stack[1024*4];

// int core1_wrapperz(int (*entry)(void), void *stack_base)
// {
//     irq_init_priorities();
//     return (*entry)();
// }
// static void __attribute__ ((naked)) core1_trampoline(void)
// {
//     __asm("pop {r0, r1, pc}");
// }

// void start(void (*entry)(void), uint32_t *stack_bottom, size_t stack_size_bytes)
// {
//     uint32_t *stack_ptr = stack_bottom + stack_size_bytes / sizeof(uint32_t);
//     // push 2 values onto top of stack for core1_trampoline
//     stack_ptr -= 3;
//     stack_ptr[0] = (uintptr_t) entry;
//     stack_ptr[1] = (uintptr_t) stack_bottom;
//     stack_ptr[2] = (uintptr_t) core1_wrapperz;
//     multicore_launch_core1_raw(core1_trampoline, stack_ptr, scb_hw->vtor);
// }

int main(void)
{
    // Overclock!
    // set_sys_clock_khz(250000, true); // 171us


    stdio_init_all();

    for (int i = 0; i <= 27; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        // gpio_set_pulls(i, false, false);
    }

    gpio_init(N64_CIC_DCLK);
    gpio_init(N64_CIC_DIO);
    gpio_init(N64_COLD_RESET);

    gpio_pull_up(N64_CIC_DIO);


    // gpio_set_dir(PICO_LA1, GPIO_OUT);
    // gpio_put(PICO_LA1, 0);
    // gpio_set_dir(PICO_LA2, GPIO_OUT);
    // gpio_put(PICO_LA2, 0);


    // Init UART on pin 28/29
    stdio_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    printf("PicoCart64 Booting!\r\n");

    // sleep_ms(10);

    // irq_set_enabled(SIO_IRQ_PROC0, false);

#ifdef PIO_MODE
    // Init PIO before starting the second core
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &cart_program);
    cart_pin_forever(pio, 0, offset);
#endif

    // FIXME: This is extremely weird, but if i launch it like this, twice, then it works??
    // start(foo, my_core1_stack, sizeof(my_core1_stack));
    multicore_launch_core1(foo);
    // multicore_launch_core1(foo);
    // multicore_launch_core1(foo);

    // cic_run();


    // Wait for reset to be released
    while (gpio_get(N64_COLD_RESET) == 0) {
        // sleep_us(1);
    }

    // printf("main 1!\r\n");


    // N64_COLD_RESET = 1, n64 booted


    // printf("main 2!\r\n");


    cart_state_t state = STATE_INIT;

    uint32_t n64_addr = 0;
    uint32_t n64_addr_h = 0;
    uint32_t n64_addr_l = 0;

    uint32_t last_addr = 0;
    uint32_t get_msb = 0;

    while (1) {

#ifdef PIO_MODE

        // TX = c code to PIO
        // RX = PIO to c code
        // pio_sm_is_tx_fifo_empty(PIO pio, uint sm)
        // pio_sm_is_rx_fifo_empty(pio, sm)

        uint32_t addr = swap16(pio_sm_get_blocking(pio, 0));

        if (addr != 0) {
            // We got a legitimate start addres
            last_addr = addr;
            get_msb = 1;
        } else {
            // We got a "Give me next 16 bits" command
            static uint32_t word;
            if (get_msb) {
                // if (addr == 0x10000000) {
                //     // word = 0x8037FF40;
                //     word = 0x40FF3780;
                // } else {
                    word = rom_file_32[(last_addr & 0xFFFFFF) >> 2];
                // }
                pio_sm_put_blocking(pio, 0, ((word << 8) & 0xFF00)  | ((word >> 8) & 0xFF));
                // pio_sm_put_blocking(pio, 0, (word & 8) | 1);
            } else {
                pio_sm_put_blocking(pio, 0, ((word >> 8) & 0xFF00)  | (word >> 24));
                // pio_sm_put_blocking(pio, 0, (word & 8) | 2);
                last_addr += 4;
            }

            get_msb = !get_msb;
        }

        // if (addr == 0x10000000) {
        //     pio_sm_put_blocking(pio, 0, 0x8037); // Data MSB
        //     pio_sm_put_blocking(pio, 0, 0xFF40); // Data LSB <- extra slow bus config
        // } else {

        //     uint32_t word = rom_file_32[(addr & 0xFFFFFF) >> 2];

        //     // TODO: Do byteswap compile time instead
        //     // 80 37 12 40 => 0x40123780 in 'word'
        //     // => 0x8037
        //     // => 0x1240

        //     pio_sm_put_blocking(pio, 0, ((word << 8) & 0xFF00)  | ((word >> 8) & 0xFF));
        //     pio_sm_put_blocking(pio, 0, ((word >> 8) & 0xFF00)  | (word >> 24));
        //     // pio_sm_put_blocking(pio, 0, (word & 8) | 1);
        //     // pio_sm_put_blocking(pio, 0, (word & 8) | 2);

        // }

        // ringbuf_put((ringbuf_entry_t) {
        //     .type = ENTRY_TYPE_ADDRESS,
        //     .address = addr,
        // });


#else





        // cic_process();

        // Always read gpios first in one go
        uint32_t all = gpio_get_all();

        switch (state) {

        case STATE_INIT:
            if (gpio_get(N64_COLD_RESET)) {
                state = update_state(state, STATE_START);
            }
            break;

        case STATE_START:
            // if (gpio_get(N64_ALEL) && gpio_get(N64_ALEH)) {
            if ((all & (1 << N64_ALEL)) && (all & (1 << N64_ALEH))) {
                // gpio_put(PICO_LA1, 0);
                // gpio_put(PICO_LA2, 0);
                state = update_state(state, STATE_WAIT_ADDR_H);
            }
            // if (!gpio_get(N64_COLD_RESET))
            //     state = update_state(state, STATE_INIT);
            break;

        case STATE_WAIT_ADDR_H:
            // if (gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
            if ((all & (1 << N64_ALEL)) && !(all & (1 << N64_ALEH))) {
                // gpio_put(PICO_LA1, 1);
                // gpio_put(PICO_LA2, 0);

                // Store 16 bits from ADxx
                // uint32_t all = gpio_get_all();
                n64_addr_h = all & 0xFFFF;

                state = update_state(state, STATE_WAIT_ADDR_L);
            }
            // if (!gpio_get(N64_COLD_RESET))
            //      state = update_state(state, STATE_INIT);
            break;

        case STATE_WAIT_ADDR_L:
            // if (!gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
            if (!(all & (1 << N64_ALEL)) && !(all & (1 << N64_ALEH))) {
                // gpio_put(PICO_LA1, 0);
                // gpio_put(PICO_LA2, 1);

                // uint32_t all = gpio_get_all();
                n64_addr_l = all & 0xFFFF;
                n64_addr = n64_addr_l | (n64_addr_h << 16);

                ringbuf_put((ringbuf_entry_t) {
                    .type = ENTRY_TYPE_ADDRESS,
                    .address = n64_addr,
                });

                // state = update_state(state, STATE_WAIT_READ_WRITE);

                // Go back and wait for a new transfer for now (we are too slow?)
                state = update_state(state, STATE_START);
            }
            // if (!gpio_get(N64_COLD_RESET))
            //     state = update_state(state, STATE_INIT);
            break;

        case STATE_WAIT_READ_WRITE:
            if (!gpio_get(N64_READ)) {
                // gpio_set_dir_out_masked(0xFFFF); // AD0 -> AD15 = OUT

                // static int ctr = 0;
                // ctr++;
                // if (ctr & 1) {
                //     gpio_put_all(0x8037);
                // } else {
                //     gpio_put_all(0x1240);
                // }

                state = update_state(state, STATE_WAIT_READ_H);
            }
            // if (!gpio_get(N64_COLD_RESET))
            //     state = update_state(state, STATE_INIT);
            break;

        case STATE_WAIT_READ_H:
            if (gpio_get(N64_READ)) {
                 state = update_state(state, STATE_WAIT_READ_WRITE);
            } else if (gpio_get(N64_ALEL) || gpio_get(N64_ALEH)) {
                // gpio_set_dir_in_masked(0xFFFF); // AD0 -> AD15 = IN
                state = update_state(state, STATE_START);
            }
            // if (!gpio_get(N64_COLD_RESET))
            //     state = update_state(state, STATE_INIT);
            break;

        } // switch

#endif
    } // while

    return 0;
}
