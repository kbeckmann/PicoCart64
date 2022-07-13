/*
 * Copyright (c) 2022 Konrad Beckmann
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdio/driver.h"
#include "hardware/gpio.h"

#include "stdio_async_uart.h"
#include "ringbuf.h"

#ifndef PICO_STDIO_UART_DEFAULT_CRLF
#define PICO_STDIO_UART_DEFAULT_CRLF PICO_STDIO_DEFAULT_CRLF
#endif

static uart_inst_t *uart_instance;

RINGBUF_CREATE(tx_rb, 1024, char);

static void on_uart_tx(void)
{
    if (tx_rb.head != tx_rb.tail) {
        // Write one byte to the UART data register
        uart_get_hw(uart_instance)->dr = *ringbuf_get_tail_ptr(tx_rb);
        tx_rb.tail = (tx_rb.tail + 1) % sizeof(tx_rb.buf);
    } else {
        // Clear all interrupts when not writing a byte
        uart_get_hw(uart_instance)->icr = UART_UARTICR_BITS;
    }
}

void stdio_uart_out_chars(const char *buf, int length)
{
    ringbuf_add_buf(tx_rb, buf, length);

    if (uart_is_writable(uart_instance)) {
        // Write one byte to the UART data register
        uart_get_hw(uart_instance)->dr = *ringbuf_get_tail_ptr(tx_rb);
        tx_rb.tail = (tx_rb.tail + 1) % sizeof(tx_rb.buf);
    }
}

static int stdio_uart_in_chars(char *buf, int length)
{
    // NOTE: This is synchronous, and will probably stay that way
    int i = 0;

    while (i < length && uart_is_readable(uart_instance)) {
        buf[i++] = uart_getc(uart_instance);
    }

    return i ? i : PICO_ERROR_NO_DATA;
}

static stdio_driver_t stdio_uart = {
    .out_chars    = stdio_uart_out_chars,
    .in_chars     = stdio_uart_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_UART_DEFAULT_CRLF
#endif
};

void stdio_async_uart_init_full(struct uart_inst *uart, uint baud_rate, int tx_pin, int rx_pin)
{
    uart_instance = uart;
    uart_init(uart_instance, baud_rate);
    if (tx_pin >= 0) gpio_set_function((uint)tx_pin, GPIO_FUNC_UART);
    if (rx_pin >= 0) gpio_set_function((uint)rx_pin, GPIO_FUNC_UART);

    uart_set_hw_flow(uart, false, false);
    uart_set_format(uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart_instance, false);

    // Configure the processor to run on_uart_tx() when UART TX is done
    int UART_IRQ = uart == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_tx);
    irq_set_enabled(UART_IRQ, true);

    // Only enable the Transmit interrupt mask.
    uart_get_hw(uart)->imsc = UART_UARTIMSC_TXIM_BITS;

    stdio_set_driver_enabled(&stdio_uart, true);
}
