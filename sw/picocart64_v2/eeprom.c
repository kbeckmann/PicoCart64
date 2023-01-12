/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eeprom.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "pio_uart/pio_uart.h"

#define JOYBUS_DAT_PIN 21
#define DEBUG_JOYBUS_SEND 1

int64_t joybus_stop_bit_timer_callback(alarm_id_t id, void *user_data);
void send_joybus_info();

typedef enum {
    INFO = 0x00,
    // CONTROLLER_STATE = 0x01,
    // READ_CONTROLLER_ACCESSORY = 0x02,
    // WRITE_CONTROLLER_ACCESSORY = 0x03,
    READ_EEPROM = 0x04,
    WRITE_EEPROM = 0x05,
    RESET = 0xFF // functions the same as INFO
} JOYBUS_COMMAND;

//typedef enum {
const uint16_t EEPROM_4K = 0x0080;
const uint16_t EEPROM_16K = 0x00C0;
//} JOYBUS_PROTOCOL_IDENTIFIER;
const uint8_t JOYBUS_EEPROM_WRITE_IN_PROGRESS = 0x80;


typedef struct {
    uint8_t cmd;
    uint8_t num_tx_bytes; // Num bytes incoming, includes the cmd 
    uint8_t num_rx_bytes; // Num bytes outgoing
} joybus_command_t;

static const joybus_command_t joybusCommands[] = {
    [INFO]          = {INFO, 1, 3},
    [READ_EEPROM]   = {READ_EEPROM, 2, 8}, // cmd, block to read, send 8 bytes data from requested block
    [WRITE_EEPROM]  = {WRITE_EEPROM, 10, 1}// cmd, block to write + data to write, is_busy (0x80 if busy 0x0 if not)
};

// 0x80 if a write is in progress
// 0 if no write in progress
static uint8_t eeprom_write_in_progress = 0;

// 4kb (512 bytes) 0-63 available blocks
// 16kb (2048 bytes) 0-255 available blocks
// In a 512 byte EEPROM, the top two bits of block number are ignored: blocks 64-255 are repeats of the first 64
static uint8_t EEPROM_MAX_NUM_BLOCKS = 64; // init to smaller eeprom size

// Size of an eeprom block in bytes
#define EEPROM_BLOCK_SIZE 8

uint16_t eeprom_type = EEPROM_4K;
uint8_t eeprom[2048]; // Init with max eeprom size

volatile uint8_t process_joybus_buf = false;

#define JOYBUS_BUFFER_SIZE 40
const uint32_t zero_bit_low_time_us = 3;
const uint32_t one_bit_low_time_us = 1;

static uint32_t lastRisingEdgeTime = 0;
static uint32_t lastFallingEdgeTime = 0;
static uint8_t joybus_buffer[JOYBUS_BUFFER_SIZE];
static uint8_t joybusBufferIndex = 0; // index of item in joybus_buffer
static uint8_t bitIndex = 0; // index of the current bit we are reading

// If flag is set, send data over uart bridge to mcu2
static void inline debug_print(uint8_t value) {
    #if DEBUG_JOYBUS_SEND == 1
    uart_tx_program_putc(value);
    #endif
}

alarm_id_t joybus_stop_bit_timer_id = -1;
int64_t joybus_stop_bit_timer_callback(alarm_id_t id, void *user_data) {
    process_joybus_buf = true;
    return 0;
}

void joybus_rising_edge(uint gpio, uint32_t events) {
    lastRisingEdgeTime = time_us_32();
    uint32_t lowTime = lastRisingEdgeTime - lastFallingEdgeTime;

    uint8_t value = 0; // TODO error handling, in case neither of these cases are true
    // If low was quick, this is a 1
    if(lowTime <= one_bit_low_time_us) {
        value = 1;

    // If low was longer, this is a zero
    } else if (lowTime <= zero_bit_low_time_us) {
        value = 0;
    }

    if (bitIndex < 8) {
        // Set bit if 1
        if (value == 1) {
            joybus_buffer[joybusBufferIndex] |= 1U << bitIndex++;
        } else {
            joybus_buffer[joybusBufferIndex] &= ~(1U << bitIndex++);
        }
    } else {
        // bit index == 8, this byte is finished
        bitIndex = 0;
        joybusBufferIndex++;
    }

    // start the stop bit timer
    joybus_stop_bit_timer_id = add_alarm_in_us(5, joybus_stop_bit_timer_callback, NULL, false);
}

void joybus_falling_edge(uint gpio, uint32_t events) {
    lastFallingEdgeTime = time_us_32();

    // stop the stop bit timer. Safe to call if we never started the alarm
    cancel_alarm(joybus_stop_bit_timer_id);
}

inline void start_joybus_irq() {
    gpio_set_irq_enabled_with_callback(JOYBUS_DAT_PIN, GPIO_IRQ_EDGE_RISE, true, &joybus_rising_edge);
    gpio_set_irq_enabled_with_callback(JOYBUS_DAT_PIN, GPIO_IRQ_EDGE_FALL, true, &joybus_falling_edge);
}

inline void stop_joybus_irq() {
    gpio_set_irq_enabled(JOYBUS_DAT_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
}

void init_joybus() {
/*
* The protocol utilizes four types of bits: Zero, One, the Console Stop Bit, and the Controller Stop Bit. 
* Zero, One, and the Controller Stop Bits are 4μs long, while the Console Stop Bit is 3μs. 
* Communication is always initiated by the console, by sending an 8 bit (one byte) command to the device plugged in (not necessarily always a standard controller). 
* A Console Stop Bit will usually follow after this command byte unless there is more data.
*/
    // fill buffer with zeros
    memset(joybus_buffer, 0, JOYBUS_BUFFER_SIZE);

    // 
    process_joybus_buf = false;
    
    start_joybus_irq(); // wait for data
}

void read_joybus() {
    process_joybus_buf = false;

    uint8_t cmd = joybus_buffer[0];
    
    debug_print(cmd);

    switch (cmd) {
    case INFO:
        send_joybus_info();
        break;
    case READ_EEPROM: 
        // Read eeprom at offset
        read_eeprom(joybus_buffer[1]);
        break;
    case WRITE_EEPROM:
        // Buffer to read from starts at offset 2, cmd=0, block=1, followed by 8 bytes of data
        write_eeprom(joybus_buffer[1], joybus_buffer+2);
        break;
    default:
        break;
    }

    debug_print(0xAA);
}

// static uint8_t is_sending_bit = 0;
// static uint32_t send_timer_id = -1;
// static uint8_t num_bytes_sent = 0;
// static uint8_t bit_to_send = 0; // TODO just use the user_data field of the timer callback

// static void end_bit_timer_callback(alarm_id_t id, void *user_data) {
//     is_sending_bit = 0;
//     gpio_put(JOYBUS_DAT_PIN, 1);
// }

static void sendBit(uint8_t bit) {
    uint32_t timeLow = bit ? one_bit_low_time_us : zero_bit_low_time_us;
    gpio_put(JOYBUS_DAT_PIN, 0); // always start low

    // Hold pin low for specified time
    busy_wait_us(timeLow);

    // Then raise the pin high
    gpio_put(JOYBUS_DAT_PIN, 1);
}

static void write_to_joybus(uint8_t* buf, uint8_t count) {
    uint8_t numBytesToSend = count;
    uint8_t byteIndex = 0;
    uint8_t bitIndex = 0;
    uint8_t bitToSend = (buf[byteIndex] >> bitIndex) & 0x01;
    while(numBytesToSend > 0) {
        sendBit(bitToSend);
        bitIndex++;

        if (bitIndex >= 8) {
            bitIndex = 0;
            byteIndex++;
            numBytesToSend--;
        }

        bitToSend = (buf[byteIndex] >> bitIndex) & 0x01;
    }
}

void send_joybus_info() {
    uint8_t* buf = malloc(1 + joybusCommands[INFO].num_rx_bytes);
    buf[0] = joybusCommands[INFO].cmd;
    buf[1] = eeprom_type;
    buf[3] = eeprom_write_in_progress;

    write_to_joybus(buf, joybusCommands[INFO].num_rx_bytes);
}

// Read eeprom at block and send that block of data
void read_eeprom(uint8_t block) {
    debug_print(block);

    uint8_t offset = block * EEPROM_BLOCK_SIZE;
    uint8_t* buf = malloc(8);
    memcpy(buf, eeprom+offset, EEPROM_BLOCK_SIZE);

    write_to_joybus(buf, EEPROM_BLOCK_SIZE);
}

// buf must be 8 bytes
void write_eeprom(uint8_t block, uint8_t* buf) {
    debug_print(block);

    uint8_t offset = block * EEPROM_BLOCK_SIZE;
    memcpy(eeprom+offset, buf, EEPROM_BLOCK_SIZE);
}