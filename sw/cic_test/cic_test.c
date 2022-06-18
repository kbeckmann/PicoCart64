/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

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

int cart(void)
{
    
}

int main()
{
    stdio_init_all();

    for (int i = 0; i <= 27; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }

    gpio_set_dir(PICO_LA1, GPIO_OUT);
    gpio_put(PICO_LA1, 0);
    gpio_set_dir(PICO_LA2, GPIO_OUT);
    gpio_put(PICO_LA2, 0);

    cic_run();

    // multicore_launch_core1(cic_run);

    cart_state_t state = STATE_INIT;

    uint32_t n64_addr = 0;
    uint32_t n64_addr_h = 0;
    uint32_t n64_addr_l = 0;

    while (1) {
        // cic_process();

        switch (state) {

        case STATE_INIT:
            if (gpio_get(N64_COLD_RESET)) {
                state = STATE_START;
                // printf("State => %d\n", state);
            }
            break;

        case STATE_START:
            if (gpio_get(N64_ALEL) && gpio_get(N64_ALEH)) {
                state = STATE_WAIT_ADDR_H;
                gpio_put(PICO_LA1, 0);
                gpio_put(PICO_LA2, 1);

                // printf("State => %d\n", state);
            }
            if (!gpio_get(N64_COLD_RESET))
                state = STATE_INIT;
            break;

        case STATE_WAIT_ADDR_H:
            if (gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
                gpio_put(PICO_LA1, 1);
                gpio_put(PICO_LA2, 0);

                // Store 16 bits from ADxx
                uint32_t all = gpio_get_all();
                // printf("n64_addr_h: %08lx\n", all);
                n64_addr_h = all & 0xFFFF;
                state = STATE_WAIT_ADDR_L;
                // printf("State => %d\n", state);
            }
            if (!gpio_get(N64_COLD_RESET))
                state = STATE_INIT;
            break;

        case STATE_WAIT_ADDR_L:
            if (!gpio_get(N64_ALEL) && !gpio_get(N64_ALEH)) {
                gpio_put(PICO_LA1, 1);
                gpio_put(PICO_LA2, 1);

                uint32_t all = gpio_get_all();
                // printf("n64_addr_l: %08lx\n", all);
                n64_addr_l = all & 0xFFFF;
                n64_addr = n64_addr_l | (n64_addr_h << 16);

                // printf("Addr: %08lx\n", n64_addr);

                state = STATE_WAIT_READ_WRITE;
                // printf("State => %d\n", state);
            }
            if (!gpio_get(N64_COLD_RESET))
                state = STATE_INIT;
            break;

        }
    }
    return 0;
}
