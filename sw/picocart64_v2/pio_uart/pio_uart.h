/**
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * Copyright (c) 2022 Kaili Hill
 */

#ifndef _PIO_UART_H
#define _PIO_UART_H

#include "hardware/pio.h"
#include "pio_uart.pio.h"

typedef struct pio_uart_inst {
    PIO pio;
    uint sm;
} pio_uart_inst_t;

#define RX_RING_BUFFER_SIZE 512
typedef struct RXRingBuffer_t {                                                                  
    uint8_t  buf[RX_RING_BUFFER_SIZE];                                                
    uint32_t head;                                                   
    uint32_t tail;                                                   
} RXRingBuffer_t;

bool rx_uart_buffer_has_data();
uint8_t rx_uart_buffer_get();
void rx_uart_buffer_reset();

// Use the rxRingBuffer to read from the uart
void pio_uart_init(uint rxPin, uint txPin);
void uart_tx_program_putc(char c);
void uart_tx_program_puts(const char *s);
char uart_rx_program_getc();
bool uart_rx_program_is_readable();
bool uart_tx_program_is_writable();

#endif
