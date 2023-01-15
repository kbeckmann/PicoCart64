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
#include "hardware/structs/systick.h"

#include "pio_uart/pio_uart.h"

#define JOYBUS_DAT_PIN 21

int64_t joybus_stop_bit_timer_callback(alarm_id_t id, void *user_data);
void send_joybus_info();
void stop_joybus_irq();

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

uint8_t commandsProcessed[16] = { 0 };
uint8_t commandsProcessedIndex = 0;

volatile uint8_t process_joybus_buf = false;

// increase for debugging
#define JOYBUS_BUFFER_SIZE 100 //40
const uint32_t zero_bit_low_time_us = 3;
const uint32_t one_bit_low_time_us = 1;

static uint32_t lastRisingEdgeTime = 0;
static uint32_t lastFallingEdgeTime = 0;
static uint8_t joybus_buffer[JOYBUS_BUFFER_SIZE];
static uint8_t joybusBufferIndex = 0; // index of item in joybus_buffer
static uint8_t bitIndex = 0; // index of the current bit we are reading

static uint8_t isInitialRise = true;
uint32_t e_l[128] = {0};
uint32_t t_l[128] = {0};
uint32_t e_i = 0;
uint8_t isProcessingCmd = true;
uint8_t numEvents = 0;

uint32_t pulse_fall_start_time = 0;
uint32_t pulse_rise_time = 0;
void joybus_pulse_callback(uint gpio, uint32_t events) {
    //uint32_t pulseTime = time_us_32();

    // Skip the artifact when the n64 is turned on while code is already running
    // if (events == GPIO_IRQ_EDGE_RISE && isInitialRise) {
    //     isInitialRise = false;
    //     return;
    // }

    numEvents++;
    if (events == 12) {
        e_l[e_i] = 4;
        t_l[e_i++] = time_us_32();//systick_hw->cvr;
        e_l[e_i] = 8;
        t_l[e_i++] = time_us_32();//systick_hw->cvr;
        numEvents++; // add an extra one here
    } else {
        e_l[e_i] = events;
        t_l[e_i++] = time_us_32();//systick_hw->cvr;
    }

    // uint32_t lowPulse  = startingTime - risingTime;
    // uint32_t highPulse = risingTime - fallingTime;
    // uint32_t diff = (lowPulse > highPulse) ? lowPulse - highPulse : highPulse - lowPulse;

    // if (numEvents == 19 && isProcessingCmd) {
    //     isProcessingCmd = false;
    //     process_joybus_buf = true;
    //     stop_joybus_irq();
    //     gpio_set_dir(JOYBUS_DAT_PIN, true);
    //     gpio_put(JOYBUS_DAT_PIN, 0); // hold low while we process
    // }
}

inline void start_joybus_irq() {
    gpio_set_irq_enabled_with_callback(JOYBUS_DAT_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &joybus_pulse_callback);
    irq_set_priority(IO_IRQ_BANK0, PICO_HIGHEST_IRQ_PRIORITY);
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

    process_joybus_buf = false;
    
    start_joybus_irq(); // wait for data

    memset(eeprom, 0xA1, 2048);

    printf("Initing joybus\n");

    systick_hw->csr = 0x5;
    systick_hw->rvr = 0x00FFFFFF;
}

static bool isFirstBit = true;
static inline bool readBit(uint8_t* stopBit) {
    uint32_t startTime = isFirstBit ? lastFallingEdgeTime : time_us_32();
    while(!gpio_get(JOYBUS_DAT_PIN)) {tight_loop_contents();}
    uint32_t lowTime = time_us_32() - startTime;

    if (lowTime < 1) {
        *stopBit = 1;
    }

    if(lowTime < 2) {
        busy_wait_us(3);
        return 1;
    } else if (lowTime >1 && lowTime <= 3) {
        busy_wait_us(1);
        return 0;
    }
    isFirstBit = false;
}

        /*
on falling edge:
    falling_time = now
    if not first:
        lo_pulse = rising_time - starting_time
        hi_pulse = falling_time - rising_time
        if lo_pulse < hi_pulse:
            new bit = 1
        else:
            new bit = 0
    starting_time = falling_time

on rising edge:
    rising_time = now
        */
void parse_irq_data() {
    uint32_t nsPerCycle = 6;

    uint32_t lastTimeStart = 0;
    uint32_t lastEvent = 0;

    uint32_t startingTime = 0;
    uint32_t fallingTime = 0;
    uint32_t risingTime = 0;
    uint8_t isFirstTime = true;
    uint8_t shouldSkipBit = false;
    uint8_t skipBit = 0;
    for(int i = 0; i < e_i; i++) {
        uint32_t t = t_l[i]; 
        uint32_t e = e_l[i]; 

        // Skip the junk 
        if (i == 0 && e == GPIO_IRQ_EDGE_RISE) {
            continue;
        }

        if (e == GPIO_IRQ_EDGE_FALL) {
            fallingTime = t;
            if (!isFirstTime && bitIndex < 8) {
                uint32_t lowPulse  = startingTime - risingTime;
                uint32_t highPulse = risingTime - fallingTime;
                if (lowPulse < highPulse) {
                    printf("1 ");
                    joybus_buffer[joybusBufferIndex] |= 1U << (7-bitIndex++);
                } else {
                    printf("0 ");
                    joybus_buffer[joybusBufferIndex] &= ~(1U << (7-bitIndex++));
                }
                if (bitIndex >= 8) {
                    bitIndex = 0;
                    joybusBufferIndex++;
                    printf("\n");
                }
            } 

            isFirstTime = false;
            startingTime = t;
        }

        if (e == GPIO_IRQ_EDGE_RISE) {
            risingTime = t;

            uint32_t lowPulse  = startingTime - risingTime;
            uint32_t highPulse = risingTime - fallingTime;
            uint32_t diff = (lowPulse > highPulse) ? lowPulse - highPulse : highPulse - lowPulse;
            if (diff < 5) {
                // stop bit?
                if (bitIndex >= 8) {
                    bitIndex = 0;
                    joybusBufferIndex++;
                    printf("\n");
                } else {
                    printf("incorrectly guessed stop bit[%d]\n", i);
                }
            }
        }
    }
}

uint8_t read_joybuf_cmd() {
    uint32_t nsPerCycle = 4;

    uint32_t lastTimeStart = 0;
    uint32_t lastEvent = 0;

    uint32_t startingTime = 0;
    uint32_t fallingTime = 0;
    uint32_t risingTime = 0;
    uint8_t isFirstTime = true;
    uint8_t isInfoByte = true;
    uint8_t didSkipJunk = false;
    uint8_t shouldSkipBit = false;
    uint8_t skipBit = 0;
    for(int i = 0; i < e_i; i++) {
        uint32_t t = t_l[i]; 
        uint32_t e = e_l[i]; 

        // Skip the junk 
        if (i == 0 && e == GPIO_IRQ_EDGE_RISE) {
            didSkipJunk = true;
            continue;
        }

        if (e == GPIO_IRQ_EDGE_FALL) {
            fallingTime = t;
            if (!isFirstTime && bitIndex < 8) {
                uint32_t lowPulse  = startingTime - risingTime;
                uint32_t highPulse = risingTime - fallingTime;
                if (lowPulse < highPulse) {
                    // printf("1 ");
                    joybus_buffer[joybusBufferIndex] |= 1U << (7-bitIndex++);
                } else {
                    // printf("0 ");
                    joybus_buffer[joybusBufferIndex] &= ~(1U << (7-bitIndex++));
                }
                if (bitIndex >= 8 && !isInfoByte) {
                    bitIndex = 0;
                    joybusBufferIndex++;
                    // printf("\n");
                }
            } else if (!isFirstTime && bitIndex == 8 && isInfoByte) {
                isInfoByte = false;

                if (bitIndex >= 8) {
                    bitIndex = 0;
                    joybusBufferIndex++;
                    printf("\n");
                }
            }

            isFirstTime = false;
            startingTime = t;
        }

        if (e == GPIO_IRQ_EDGE_RISE) {
            risingTime = t;
        }
    }
}

void read_joybus() {
    process_joybus_buf = false;

    // Read data until stop bit
    // readData();
    parse_irq_data();
    // joybusBufferIndex = 0;
    // bitIndex = 0;
    isFirstBit = true;

    uint8_t cmd = joybus_buffer[0];
    commandsProcessed[commandsProcessedIndex++] = cmd;
    if (commandsProcessedIndex >= 16) {
        commandsProcessedIndex = 0;
    }

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
    // isProcessingCmd = true;
    // numEvents = 0;
    // e_i = 0;
}

static void inline sendBit(uint8_t bit) {
    uint32_t timeLow = bit ? one_bit_low_time_us : zero_bit_low_time_us;
    uint32_t timeHigh = bit ? zero_bit_low_time_us : one_bit_low_time_us; // inverse 
    gpio_put(JOYBUS_DAT_PIN, 0); // always start low

    // Hold pin low for specified time
    busy_wait_us(timeLow);

    // Then raise the pin high
    gpio_put(JOYBUS_DAT_PIN, 1);

    busy_wait_us(timeHigh);
}

static void inline sendStopBit() {
    gpio_put(JOYBUS_DAT_PIN, 0); // always start low

    // Hold pin low for specified time
    busy_wait_us(1);

    // Then raise the pin high
    gpio_put(JOYBUS_DAT_PIN, 1);

    busy_wait_us(2);
}

uint8_t bitsSent[128] = {0};
uint8_t bitsSentIndex = 0;
static void write_to_joybus(uint8_t* buf, uint8_t count) {
    // disable irq
    stop_joybus_irq();
    // Set to output
    gpio_set_dir(JOYBUS_DAT_PIN, true);

    uint8_t numBytesToSend = count;
    uint8_t byteIndex = 0;
    gpio_put(JOYBUS_DAT_PIN, 1); // release the hold
    do {
        for (int i = 0; i < 8; i++) {
            uint8_t bitToSend = ((1 << (i % 8)) & (buf[byteIndex])) >> (i % 8);
            sendBit(bitToSend);
            bitsSent[bitsSentIndex++] = bitToSend;
        }
        byteIndex++;
        numBytesToSend--;
    } while (numBytesToSend > 0);

    // send stop bit
    sendStopBit();

    // Set back to input and restart the irq
    gpio_set_dir(JOYBUS_DAT_PIN, false);
    start_joybus_irq();
}

void send_joybus_info() {
    printf("Sending joybus info...\n");
    uint8_t* buf = malloc(joybusCommands[INFO].num_rx_bytes);
    buf[0] = (uint8_t)(eeprom_type >> 8);
    buf[1] = (uint8_t)(eeprom_type);
    buf[2] = eeprom_write_in_progress;
    write_to_joybus(buf, joybusCommands[INFO].num_rx_bytes);
}

uint32_t numReads = 0;
uint32_t numWrites = 0;
// Read eeprom at block and send that block of data
void read_eeprom(uint8_t block) {
    numReads++;
    uint8_t offset = block * EEPROM_BLOCK_SIZE;
    uint8_t* buf = malloc(8);
    memcpy(buf, eeprom+offset, EEPROM_BLOCK_SIZE);

    write_to_joybus(buf, EEPROM_BLOCK_SIZE);
}

// buf must be 8 bytes
void write_eeprom(uint8_t block, uint8_t* buf) {
    numWrites++;
    uint8_t offset = block * EEPROM_BLOCK_SIZE;
    memcpy(eeprom+offset, buf, EEPROM_BLOCK_SIZE);
}

static uint32_t lastPrintIndex = 0;
static uint32_t lastTimePrintIndex = 0;
void joybus_dump_debug_data() {
    for (int i = lastPrintIndex; i < commandsProcessedIndex; i++) {
        printf("cmd[%d]: %02x\n", i, commandsProcessed[i]);
    }

    uint32_t lastT = 0;
    printf("Num events: %u\n", numEvents);
    for(int i = 0; i < e_i; i ++) {
        //uint32_t diffCycles = (lastT - t_l[i]);
        uint32_t diffCycles = (t_l[i] - lastT);
        printf("e: %u, t: %u, d: %fus [%u]\n", e_l[i], t_l[i], ((diffCycles * 3.76) / 1000.0), diffCycles);
        lastT = t_l[i];
    }

    for (int i = 0; i < joybusBufferIndex; i++) {
        printf("%02x ", joybus_buffer[i]);
    }
    printf("\n");

    for(int i = 0; i < bitsSentIndex; i++) {
        printf("%u ", bitsSent[i]);
    }
    
    lastPrintIndex = commandsProcessedIndex;
}