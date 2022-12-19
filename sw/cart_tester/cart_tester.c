/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <tusb.h>

#include "cart_tester_pins.h"
#include "cart_tester.h"

#define LATCH_DELAY_MULTIPLYER 10
#define LATCH_DELAY_US 4 // Used for reads
#define LATCH_DELAY_NS (110 / 7) * LATCH_DELAY_MULTIPLYER // Used for sending addresses. 133mhz is 7.5NS, let's just use int math though

#define CART_ADDRESS_START 0x10000008
#define CART_ADDRESS_UPPER_RANGE 0x1FBFFFFF

uint32_t finalReadAddress = CART_ADDRESS_START + 0x10; //0x200000;
uint32_t address_pin_mask = 0;

void set_ad_input() {
    for(int i = 0; i < 16; i++) {
        gpio_set_dir(i, false);
    }
}

void set_ad_output() {
    for(int i = 0; i < 16; i++) {
        gpio_set_dir(i, true);
    }
}

void main() {
    /*
     * Picocart init waits for both ALEH and ALEL to go high before waiting for them to go low 
     * 
     * RESET should be low until we are ready to start
     * 
     * 
     * SEND ADDRESS 
     * * aleH HIGH
     * * aleL HIGH
     * * aleH LOW
     *      Send high 16 bits
     * * aleL LOW
     *      Send low 16 bits
     *
     * 
     * READ DATA
     * * READ LOW
     * * aleH LOW
     *      get high 16 bits data
     * * aleL LOW 
     *      get low 16 bits data
     */

    stdio_init_all();

    // Setup the LED pin
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    volatile int t = 0;
    // while(!tud_cdc_connected()) {
    while(t < 3) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        sleep_ms(100);
        t++;
    }

    gpio_put(PICO_DEFAULT_LED_PIN, true);
    sleep_ms(100);
    printf("\n\nSetting up cart tester\n");

    // setup data/address lines
    for (int i = 0; i < 16; i++) {
        gpio_init(i);
        gpio_set_dir(i, true); // set output initially
        gpio_set_function(i, GPIO_FUNC_SIO);
    }

    gpio_init(N64_ALEH);
    gpio_set_dir(N64_ALEH, true); // set output initially
    
    gpio_init(N64_ALEL);
    gpio_set_dir(N64_ALEL, true); // set output initially
    
    gpio_init(N64_READ);
    gpio_set_dir(N64_READ, true); // set output initially
    
    gpio_init(N64_COLD_RESET);
    gpio_set_dir(N64_COLD_RESET, true); // set output initially
    gpio_put(N64_COLD_RESET, false); // roms won't read until this is true

    // create a gpio mask
    for(int i = 0; i < 16; i++) {
        address_pin_mask |= 1 << i;
    }

    // // wait for serial input before starting
    // gpio_put(PICO_DEFAULT_LED_PIN, false);
    // int buffer[64];
    // int count = 0;
    // volatile bool waiting = true;
    // while(waiting) {
    //     process_serial_command:
    //     tight_loop_contents();

    //     volatile int c = getchar();
    //     buffer[count++] = c;

    //     if (c == '\n') {
    //         // process input
    //         switch (buffer[0]) {
    //         case '1': 
    //             printf("Start read\n");
    //             sleep_ms(100);
    //             waiting = false;
    //             break;
                
    //         case '2':
    //             printf("%c\n", buffer[0]);
    //             break;

    //         case '3':
    //             gpio_put(PICO_DEFAULT_LED_PIN, false);
    //             sleep_ms(100);
    //             gpio_put(PICO_DEFAULT_LED_PIN, true);
    //             sleep_ms(100);
    //             gpio_put(PICO_DEFAULT_LED_PIN, false);
    //             sleep_ms(100);
    //             gpio_put(PICO_DEFAULT_LED_PIN, true);
    //             // printf("Boop\n");
    //             break;

    //         default: 
    //             gpio_put(PICO_DEFAULT_LED_PIN, false);
    //             sleep_ms(30);
    //             gpio_put(PICO_DEFAULT_LED_PIN, true);
    //             sleep_ms(30);
    //             gpio_put(PICO_DEFAULT_LED_PIN, false);
    //             sleep_ms(30);
    //             gpio_put(PICO_DEFAULT_LED_PIN, true);
    //             break;
    //         }
    //         buffer[0] = -1;
    //         count = 0;
    //     }

    //     if (count >= 64) {
    //         count = 0;
    //     }
    // }

    sleep_ms(100);
    
    printf("READ_START\n");

    sleep_ms(100);

    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Start sending addresses and getting data
    gpio_put(N64_COLD_RESET, true);
    
    uint32_t address = CART_ADDRESS_START; // Starting address
    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        
        // Send address, sends high 16 for LATCH_DELAY_US, sends low 16
        send_address(address);

        sleep_ms(10);

        // Clear mask?
        gpio_clr_mask(address_pin_mask);
        send_address(0); // Tell it we want to read

        sleep_ms(10);

        // Not sure if we need another latch delay but the timing diagram has 7us between read going high and 
        busy_wait_us(LATCH_DELAY_US);
        
        // Clear mask?
        gpio_clr_mask(address_pin_mask);

        uint16_t data = start_read();

        sleep_ms(10);

        verify_data(data, address);
        
        address += 2; // increment address by 2 bytes

        // make sure we don't go out of bounds
        if (address > finalReadAddress) {
            // FINISHED
            break;
        }
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    printf("END\n");

    gpio_put(PICO_DEFAULT_LED_PIN, true);
    
    // Go back to waiting for serial input, in case we want to run again
    // waiting = true;
    gpio_put(N64_COLD_RESET, false); // roms won't read until this is true
    // printf("RESTARTING\n");
    // goto process_serial_command;
    sleep_ms(1000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    while(1) {
        tight_loop_contents();
    }
}

static inline uint32_t swap8(uint16_t value)
{
	// 0x1122 => 0x2211
	return (value << 8) | (value >> 8);
}

void send_address(uint32_t address) {
    // Clear mask?
    gpio_clr_mask(address_pin_mask);

    gpio_put(N64_READ, true);
    gpio_put(N64_ALEH, true);
    gpio_put(N64_ALEL, true);

    // translate upper 16 bits to gpio 
    uint16_t high16 = address >> 16;
    
    // printf("High16(%08x): %08x\n", high16, (sio_hw->gpio_out ^ high16) & address_pin_mask);
    gpio_put_masked(address_pin_mask, high16);
    
    // Leave the high 16 bits on the line for at least this long
    busy_wait_at_least_cycles(LATCH_DELAY_NS); 

    // Set aleH low to send the lower 16 bits
    gpio_put(N64_ALEH, false);
    
    // Clear mask?
    gpio_clr_mask(address_pin_mask);

    // now the lower 16 bits 
    uint16_t low16 = address;
    // printf("Low16(%08x): %08x\n", low16, (sio_hw->gpio_out ^ low16) & address_pin_mask);
    gpio_put_masked(address_pin_mask, low16);

    // Leave the low 16 bits on the line for at least this long
    busy_wait_at_least_cycles(LATCH_DELAY_NS);

    // set aleL low to tell the cart we are done sending the address
    gpio_put(N64_ALEL, false);
}

uint16_t start_read() {
    
    // Set to input
    set_ad_input();

    gpio_put(N64_READ, false);
    busy_wait_us(LATCH_DELAY_US);

    // sample gpio for high16
    uint32_t high16 = gpio_get_all();

    // debug code to sample forever since picocart seems to not be returning anything
    // volatile int looping = 1;
    // while(looping > 0) {
    //     looping++;

    //     uint16_t sample = (uint16_t)gpio_get_all();
    //     if (sample != 0) {
    //         high16 = sample;
    //         printf("non zero\n");
    //         looping = 0;
    //     }
    // }

    // Finished reading
    gpio_put(N64_READ, true);
    busy_wait_us(LATCH_DELAY_US);

    printf("%08x\n", high16);

    set_ad_output();

    // return the value
    return (uint16_t)high16;
}

void verify_data(uint16_t data, uint32_t address) {
    printf("[%08x] %04x\n");
}