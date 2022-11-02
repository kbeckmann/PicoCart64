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

void pio_uart_init(irq_handler_t rx_handler, uint rxPin, uint txPin);
void uart_tx_program_putc(char c);
void uart_tx_program_puts(const char *s);
char uart_rx_program_getc();
char uart_rx_program_is_readable();

#endif
