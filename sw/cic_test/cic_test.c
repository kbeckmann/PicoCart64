/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "cic_test.pio.h"

#include "cic.h"
#include "n64.h"

#define PICO_LA1  (26)
#define PICO_LA2  (27)



typedef enum {
    STATE_INIT = 0,
    STATE_START,
    STATE_WAIT_ADDR_H,
    STATE_WAIT_ADDR_L,
    STATE_WAIT_READ_WRITE,
    STATE_WAIT_READ_H,
    STATE_WAIT_WRITE_H,
} cart_state_t;

uint32_t ringbuf[10];
uint32_t ringbuf_idx;

void ringbuf_put(uint32_t value)
{
    if (ringbuf_idx > sizeof(ringbuf) / 4) {
        for (int i = 0; i < sizeof(ringbuf) / 4; i++) {
            printf("%d: %08lX\n", i, ringbuf[i]);
        }
        ringbuf_idx = 0;
    }

    ringbuf[ringbuf_idx++] = value;
}


void cart_pin_forever(PIO pio, uint sm, uint offset) {
    cart_program_init(pio, sm, offset);
    pio_sm_set_enabled(pio, sm, true);

    // pio->txf[sm] = (clock_get_hz(clk_sys) / (2 * freq)) - 3;
}


int main()
{
    stdio_init_all();

    for (int i = 0; i <= 27; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }

    // gpio_set_dir(PICO_LA1, GPIO_OUT);
    // gpio_put(PICO_LA1, 0);
    // gpio_set_dir(PICO_LA2, GPIO_OUT);
    // gpio_put(PICO_LA2, 0);



    cic_run();

    // N64_COLD_RESET = 1, n64 booted

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &cart_program);
    cart_pin_forever(pio, 0, offset);

    // pio_sm_put_blocking(pio, 0, 0x00000000); // AD0 -> AD15 = IN


    // multicore_launch_core1(cic_run);

    cart_state_t state = STATE_INIT;

    uint32_t n64_addr = 0;
    uint32_t n64_addr_h = 0;
    uint32_t n64_addr_l = 0;

    uint8_t tmp[256];

    while (1) {

        uint32_t data = pio_sm_get_blocking(pio, 0);
        // pio_sm_put_blocking(pio, 0, 0xFFFF); // Data MSB
        // pio_sm_put_blocking(pio, 0, 0xFFFF); // Data LSB

        pio_sm_put_blocking(pio, 0, 0x8037); // Data MSB
        pio_sm_put_blocking(pio, 0, 0x1240); // Data LSB


        // ringbuf_put(data);






        // cic_process();

        // switch (state) {

        // case STATE_INIT:
        //     if (gpio_get(N64_COLD_RESET)) {
        //         state = STATE_START;
        //         // printf("State => %d\n", state);
        //     }
        //     break;

        // case STATE_START:
        //     if (gpio_get(N64_ALEL) && gpio_get(N64_ALEH)) {
        //         state = STATE_WAIT_ADDR_H;
        //         // gpio_put(PICO_LA1, 0);
        //         // gpio_put(PICO_LA2, 1);

        //         // printf("State => %d\n", state);
        //     }
        //     if (!gpio_get(N64_COLD_RESET))
        //         state = STATE_INIT;
        //     break;

        // case STATE_WAIT_ADDR_H:
        //     if (gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
        //         // gpio_put(PICO_LA1, 1);
        //         // gpio_put(PICO_LA2, 0);

        //         // Store 16 bits from ADxx
        //         uint32_t all = gpio_get_all();
        //         // printf("n64_addr_h: %08lx\n", all);
        //         n64_addr_h = all & 0xFFFF;
        //         state = STATE_WAIT_ADDR_L;
        //         // printf("State => %d\n", state);
        //     }
        //     if (!gpio_get(N64_COLD_RESET))
        //         state = STATE_INIT;
        //     break;

        // case STATE_WAIT_ADDR_L:
        //     if (!gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
        //         // gpio_put(PICO_LA1, 1);
        //         // gpio_put(PICO_LA2, 1);

        //         uint32_t all = gpio_get_all();
        //         // printf("n64_addr_l: %08lx\n", all);
        //         n64_addr_l = all & 0xFFFF;
        //         n64_addr = n64_addr_l | (n64_addr_h << 16);

        //         // printf("Addr: %08lx\n", n64_addr);

        //         state = STATE_WAIT_READ_WRITE;
        //         // printf("State => %d\n", state);
        //     }
        //     if (!gpio_get(N64_COLD_RESET))
        //         state = STATE_INIT;
        //     break;

        // }
    }
    return 0;
}
