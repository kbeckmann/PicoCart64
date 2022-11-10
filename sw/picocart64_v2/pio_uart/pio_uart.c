/**
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2022 Kaili Hill
 */
#include <stdio.h>
#include "pio_uart.h"
#include "pins_mcu1.h"

pio_uart_inst_t uart_rx = {
        .pio = pio1,
        .sm = 0
};

pio_uart_inst_t uart_tx = {
        .pio = pio1,
        .sm = 1
};

RXRingBuffer_t rxRingBuffer = {};
void rx_uart_interrupt() {
    while(uart_rx_program_is_readable()) {
        char c = uart_rx_program_getc();
        rxRingBuffer.buf[rxRingBuffer.head++] = c;
        if (rxRingBuffer.head == RX_RING_BUFFER_SIZE) {
            rxRingBuffer.head = 0;
        }
    }
}

bool rx_uart_buffer_has_data() {
    return rxRingBuffer.tail != rxRingBuffer.head;
}

uint8_t rx_uart_buffer_get() {
    uint8_t ret = rxRingBuffer.buf[rxRingBuffer.tail++];
    if (rxRingBuffer.tail >= RX_RING_BUFFER_SIZE) {
        rxRingBuffer.tail = 0;
    }
    return ret;
}

void rx_uart_buffer_reset() {
    rxRingBuffer.head = 0;
    rxRingBuffer.tail = 0;
}

void pio_uart_init(uint rxPin, uint txPin) {
	uint pioUartRXOffset = pio_add_program(uart_rx.pio, &uart_rx_program);
	uint pioUartTXOffset = pio_add_program(uart_tx.pio, &uart_tx_program);

	uart_rx_program_init(uart_rx.pio, uart_rx.sm, pioUartRXOffset, rxPin, PIO_UART_BAUD_RATE);
	uart_tx_program_init(uart_tx.pio, uart_tx.sm, pioUartTXOffset, txPin, PIO_UART_BAUD_RATE);

    // Finally, set irq for rx
    irq_set_exclusive_handler(PIO1_IRQ_0, rx_uart_interrupt);
	pio_set_irq0_source_enabled(pio1, pis_sm0_rx_fifo_not_empty, true);
    irq_set_enabled(PIO1_IRQ_0, true);  
}

void uart_tx_program_putc(char c) {
    pio_sm_put_blocking(uart_tx.pio, uart_tx.sm, (uint32_t)c);
}

void uart_tx_program_puts(const char *s) {
    while (*s)
        uart_tx_program_putc(*s++);
}

char uart_rx_program_getc() {
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    io_rw_8 *rxfifo_shift = (io_rw_8*)&uart_rx.pio->rxf[uart_rx.sm] + 3;
    while (pio_sm_is_rx_fifo_empty(uart_rx.pio, uart_rx.sm))
        tight_loop_contents();
    return (char)*rxfifo_shift;
}

// If there is data in the rx fifo, then return true
bool uart_rx_program_is_readable() {
    return !pio_sm_is_rx_fifo_empty(uart_rx.pio, uart_rx.sm);
}

bool uart_tx_program_is_writable() {
    return !pio_sm_is_tx_fifo_full(uart_tx.pio, uart_tx.sm);
}
