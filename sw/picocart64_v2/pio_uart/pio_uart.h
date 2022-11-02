/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_SPI_H
#define _PIO_SPI_H

#include "hardware/pio.h"
#include "pio_uart.pio.h"

typedef struct pio_uart_inst {
    PIO pio;
    uint sm;
} pio_uart_inst_t;

// void pio_uart_write8_blocking(const pio_uart_inst_t *uart, const uint8_t *src, size_t len);
// void pio_uart_read8_blocking(const pio_uart_inst_t *uart, uint8_t *dst, size_t len);
// void pio_uart_write8_read8_blocking(const pio_uart_inst_t *uart, uint8_t *src, uint8_t *dst, size_t len);

void pio_uart_init(irq_handler_t rx_handler, uint rxPin, uint txPin);
void uart_tx_program_putc(PIO pio, uint sm, char c);
void uart_tx_program_puts(const char *s);
char uart_rx_program_getc(PIO pio, uint sm);
char uart_rx_program_is_readable();

#endif
