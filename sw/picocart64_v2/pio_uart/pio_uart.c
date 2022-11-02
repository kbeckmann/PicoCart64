/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

void pio_uart_init(irq_handler_t rx_handler, uint rxPin, uint txPin) {
	uint pioUartRXOffset = pio_add_program(uart_rx.pio, &uart_rx_program);
	uint pioUartTXOffset = pio_add_program(uart_tx.pio, &uart_tx_program);

	uart_rx_program_init(uart_rx.pio, uart_rx.sm, pioUartRXOffset, rxPin, PIO_UART_BAUD_RATE);
	uart_tx_program_init(uart_tx.pio, uart_tx.sm, pioUartTXOffset, txPin, PIO_UART_BAUD_RATE);

    // Finally, set irq for rx
    //irq_set_exclusive_handler(PIO1_IRQ_0, rx_handler);
	//pio_set_irq0_source_enabled(pio1, pis_sm0_rx_fifo_not_empty, true);
}

// void uart_tx_program_putc(char c) {
//     pio_sm_put_blocking(uart_tx.pio, uart_tx.sm, (uint32_t)c);
// }

// void uart_tx_program_puts(const char *s) {
//     while (*s)
//         uart_tx_program_putc(*s++);
// }
void uart_tx_program_puts(const char *s) {}

void uart_tx_program_putc(PIO pio, uint sm, char c) {
    pio_sm_put_blocking(pio, sm, (uint32_t)c);
}

// char uart_rx_program_getc() {
//     // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
//     io_rw_8 *rxfifo_shift = (io_rw_8*)uart_rx.pio->rxf[uart_rx.sm] + 3;
//     while (pio_sm_is_rx_fifo_empty(uart_rx.pio, uart_rx.sm))
//         tight_loop_contents();
//     return (char)*rxfifo_shift;
// }

char uart_rx_program_getc(PIO pio, uint sm) {
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    io_rw_8 *rxfifo_shift = (io_rw_8*)&pio->rxf[sm] + 3;
    while (pio_sm_is_rx_fifo_empty(pio, sm))
        tight_loop_contents();
    return (char)*rxfifo_shift;
}

// If there is data in the rx fifo, then return true
char uart_rx_program_is_readable() {
    return !pio_sm_is_rx_fifo_empty(uart_rx.pio, uart_rx.sm);
}

// Modified code from the SPI example code
// // Just 8 bit functions provided here. The PIO program supports any frame size
// // 1...32, but the software to do the necessary FIFO shuffling is left as an
// // exercise for the reader :)
// //
// // Likewise we only provide MSB-first here. To do LSB-first, you need to
// // - Do shifts when reading from the FIFO, for general case n != 8, 16, 32
// // - Do a narrow read at a one halfword or 3 byte offset for n == 16, 8
// // in order to get the read data correctly justified. 

// void __time_critical_func(pio_uart_write8_blocking)(const pio_uart_inst_t *uart, const uint8_t *src, size_t len) {
//     size_t tx_remain = len, rx_remain = len;
//     // Do 8 bit accesses on FIFO, so that write data is byte-replicated. This
//     // gets us the left-justification for free (for MSB-first shift-out)
//     io_rw_8 *txfifo = (io_rw_8 *) &uart->pio->txf[uart->sm];
//     io_rw_8 *rxfifo = (io_rw_8 *) &uart->pio->rxf[uart->sm];
//     while (tx_remain || rx_remain) {
//         if (tx_remain && !pio_sm_is_tx_fifo_full(uart->pio, uart->sm)) {
//             *txfifo = *src++;
//             --tx_remain;
//         }
//         if (rx_remain && !pio_sm_is_rx_fifo_empty(uart->pio, uart->sm)) {
//             (void) *rxfifo;
//             --rx_remain;
//         }
//     }
// }

// void __time_critical_func(pio_uart_read8_blocking)(const pio_uart_inst_t *uart, uint8_t *dst, size_t len) {
//     size_t tx_remain = len, rx_remain = len;
//     io_rw_8 *txfifo = (io_rw_8 *) &uart->pio->txf[uart->sm];
//     io_rw_8 *rxfifo = (io_rw_8 *) &uart->pio->rxf[uart->sm];
//     while (tx_remain || rx_remain) {
//         if (tx_remain && !pio_sm_is_tx_fifo_full(uart->pio, uart->sm)) {
//             *txfifo = 0;
//             --tx_remain;
//         }
//         if (rx_remain && !pio_sm_is_rx_fifo_empty(uart->pio, uart->sm)) {
//             *dst++ = *rxfifo;
//             --rx_remain;
//         }
//     }
// }

// void __time_critical_func(pio_uart_write8_read8_blocking)(const pio_uart_inst_t *uart, uint8_t *src, uint8_t *dst,
//                                                          size_t len) {
//     size_t tx_remain = len, rx_remain = len;
//     io_rw_8 *txfifo = (io_rw_8 *) &uart->pio->txf[uart->sm];
//     io_rw_8 *rxfifo = (io_rw_8 *) &uart->pio->rxf[uart->sm];
//     while (tx_remain || rx_remain) {
//         if (tx_remain && !pio_sm_is_tx_fifo_full(uart->pio, uart->sm)) {
//             *txfifo = *src++;
//             --tx_remain;
//         }
//         if (rx_remain && !pio_sm_is_rx_fifo_empty(uart->pio, uart->sm)) {
//             *dst++ = *rxfifo;
//             --rx_remain;
//         }
//     }
// }
