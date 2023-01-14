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

#define DEBUG_PRINT_RAW_READ_DATA 0

#define LATCH_DELAY_MULTIPLYER 1
#define LATCH_DELAY_US 4 * LATCH_DELAY_MULTIPLYER // Used for reads
#define LATCH_DELAY_NS (110 / 7) * LATCH_DELAY_MULTIPLYER // Used for sending addresses. 133mhz is 7.5NS, let's just use int math though

#define CART_ADDRESS_START (0x10000000 + 24 * 1024 * 1024)
#define CART_ADDRESS_UPPER_RANGE 0x1FBFFFFF

uint32_t finalReadAddress = CART_ADDRESS_START + 0x16; //0x200000;
uint32_t address_pin_mask = 0;

void set_ad_input() {
    for(int i = 0; i < 16; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        // gpio_set_pulls(i, false, true);
    }
}

void set_ad_output() {
    for(int i = 0; i < 16; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        // gpio_set_pulls(i, false, true);
    }
}

void dump_rom_test() {
    sleep_ms(100);
    printf("\nDUMP READ TEST START\n");
    sleep_ms(300);

    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Start sending addresses and getting data
    gpio_put(N64_COLD_RESET, true);

    uint32_t buf_start_address = CART_ADDRESS_START;    
    uint32_t address = CART_ADDRESS_START; // Starting address
    int addressIncrement = 2;
    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        
        // printf("\nSTART SEND ADDRESS\n");
        // sleep_ms(30);

        // Send address, sends high 16 for LATCH_DELAY_US, sends low 16
        send_address(address);
        
        // Read data
        // volatile uint32_t data = read32();
        volatile uint16_t data = read16();

        // Make sure that the cart is held in a waiting patter until we verify the data
        gpio_put(N64_READ, true);
        gpio_put(N64_ALEH, true);
        gpio_put(N64_ALEL, true);

        // verify_data(data, address);
        // printf("[%08x] %08x\n", address, data);
        printf("[%08x] %04x\n", address, data);
        
        address += addressIncrement; // increment address by 2 bytes

        // Delay to make sure USB prints all the data. If it goes too fast sometimes it won't print properly.
        sleep_ms(100);

        // make sure we don't go out of bounds
        if (address > finalReadAddress) {
            // FINISHED
            break;
        }
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    printf("END\n");

    gpio_clr_mask(address_pin_mask);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    gpio_put(N64_COLD_RESET, false); // roms won't read until this is true
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while(1) {
        tight_loop_contents();
    }
}

void board_test() {
    sleep_ms(100);
    printf("Stating in 3...");
    sleep_ms(1000);
    printf("2...");
    sleep_ms(1000);
    printf("1...\n");
    sleep_ms(1000);
    printf("START board_test\n");

    // Set true to start test
    gpio_put(N64_COLD_RESET, true);

    // send values over each gpio line
    for(int i = 0; i < 16; i++) {
        printf("Sending value on AD [%d]\n", i);
		gpio_put(i, true);
        sleep_ms(50);
    }

    gpio_put(N64_ALEH, true);
    sleep_ms(50);

    gpio_put(N64_ALEL, true);
    sleep_ms(50);

    gpio_put(N64_READ, true);
    sleep_ms(50);

    bool goodLines[16];
    int numBadLines = 0;
    printf("START Read data lines from cart...\n");
    sleep_ms(10);

    for(int i = 0; i < 16; i++) {
        gpio_set_dir(i, false);
    }

    uint32_t allPins = gpio_get_all();
    printf("All pins sample: %08x\n", allPins);

    for(int i = 0; i < 16; i++) {
        printf("Checking AD [%d]... ");

        int value = 0;
        do {
            value = gpio_get(i);
        } while (value == false); 

        if (value == true) {
            printf("AD [%d] good!\n", i);
            sleep_ms(10);
        } else {
            numBadLines++;
        }

        goodLines[i] = value;
    }

    gpio_put(N64_READ, true);// leave high once finished
    gpio_put(N64_COLD_RESET, false); // reset for the next run

    printf("Num bad lines found = %d\n", numBadLines);
    printf("Testing finished!\n");
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
     * * aleL LOW
     *      get 16 bits data
     * * READ HIGH
     */

    stdio_init_all();

    // Setup the LED pin
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    // Flash the led
    volatile int t = 0;
    while(t < 3) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        sleep_ms(100);
        t++;
    }

    gpio_put(PICO_DEFAULT_LED_PIN, true);
    printf("\n\nSetting up cart tester\n");
    sleep_ms(500);

    // setup data/address lines
    for (int i = 0; i < 16; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT); // set output initially
        gpio_set_pulls(i, false, true);
        gpio_set_function(i, GPIO_FUNC_SIO);
    }

    gpio_init(N64_ALEH);
    gpio_set_dir(N64_ALEH, true); // set output initially
    gpio_put(N64_ALEH, true);
    gpio_set_pulls(N64_ALEH, true, false);
    
    gpio_init(N64_ALEL);
    gpio_set_dir(N64_ALEL, true); // set output initially
    gpio_put(N64_ALEL, false);
    gpio_set_pulls(N64_ALEL, true, false);
    
    gpio_init(N64_READ);
    gpio_set_dir(N64_READ, true); // set output initially
    gpio_put(N64_READ, true);
    gpio_set_pulls(N64_READ, true, false);

    gpio_init(N64_WRITE);
    gpio_set_dir(N64_WRITE, true);
    gpio_put(N64_WRITE, true); // 
    gpio_set_pulls(N64_WRITE, true, false);
    
    gpio_init(N64_COLD_RESET);
    gpio_set_dir(N64_COLD_RESET, true); // set output initially
    gpio_put(N64_COLD_RESET, false); // roms won't read until this is true

    // create a gpio mask
    for(int i = 0; i < 16; i++) {
        address_pin_mask |= 1 << i;
    }

    // board_test();

    dump_rom_test();

    // Run forever
    while(1);;
}

void send_address(uint32_t address) {
    // Clear mask
    gpio_clr_mask(address_pin_mask);

    gpio_put(N64_READ, true);
    gpio_put(N64_ALEH, true);
    gpio_put(N64_ALEL, true);

    // translate upper 16 bits to gpio 
    uint16_t high16 = address >> 16;
    gpio_put_masked(address_pin_mask, high16);

    // Leave the high 16 bits on the line for at least this long
    busy_wait_at_least_cycles(LATCH_DELAY_NS); 
    
    // Set aleH low to send the lower 16 bits
    gpio_put(N64_ALEH, false);
    
    // Clear mask?
    gpio_clr_mask(address_pin_mask);

    // now the lower 16 bits 
    uint16_t low16 = address;
    gpio_put_masked(address_pin_mask, low16);

    // Leave the low 16 bits on the line for at least this long
    busy_wait_at_least_cycles(LATCH_DELAY_NS);

    // set aleL low to tell the cart we are done sending the address
    gpio_put(N64_ALEL, false);

    busy_wait_at_least_cycles(40); // wait ~600ns

    // clear the mask so there is nothing on the lines
    gpio_clr_mask(address_pin_mask);
}

uint32_t read32() {
    
    // printf("START READ\n");
    // Set to input
    set_ad_input();

    gpio_put(N64_READ, false);
    sleep_us(310 / 7);

    // sample gpio for high16
    uint32_t high16 = gpio_get_all();

    // Pulse the read pin
    busy_wait_at_least_cycles(LATCH_DELAY_NS);
    gpio_put(N64_READ, true);    
    gpio_put(N64_READ, false);

    // Grab the next 16bits
    uint32_t low16 = gpio_get_all();

    #if DEBUG_PRINT_RAW_READ_DATA == 1
    printf("%04x %04x\n", (uint16_t)high16, (uint16_t)low16);
    #endif

    set_ad_output();

    // return the value
    return ((high16 & 0xFFFF) << 16) | (low16 & 0xFFFF);
}

uint16_t read16() {
    // Set to input
    set_ad_input();

    gpio_put(N64_READ, false);
    sleep_us(310 / 7);

    // sample gpio for high16
    uint16_t high16 = (uint16_t)gpio_get_all();

    // Pulse the read pin
    busy_wait_at_least_cycles(LATCH_DELAY_NS);
    gpio_put(N64_READ, true);    

    #if DEBUG_PRINT_RAW_READ_DATA == 1
    printf("%04x\n", (uint16_t)high16);
    #endif

    // Set AD pins back to output
    set_ad_output();

    // return the value
    return high16;
}

void verify_data(uint32_t data, uint32_t address) {
    printf("[%08x] %08x\n", address, data);
}