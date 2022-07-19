/*
 * Copyright (c) 2022 Konrad Beckmann
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_STDIO_UART_H
#define _PICO_STDIO_UART_H

#include "pico/stdio.h"
#include "hardware/uart.h"

/** \brief Support for stdin/stdout using UART
 *  \defgroup pico_stdio_uart pico_stdio_uart
 *  \ingroup pico_stdio
 *
 *  Linking this library or calling `pico_enable_stdio_uart(TARGET ENABLED)` in the CMake (which
 *  achieves the same thing) will add UART to the drivers used for standard input/output
 */

// PICO_CONFIG: PICO_STDIO_UART_DEFAULT_CRLF, Default state of CR/LF translation for UART output, type=bool, default=PICO_STDIO_DEFAULT_CRLF, group=pico_stdio_uart
#ifndef PICO_STDIO_UART_DEFAULT_CRLF
#define PICO_STDIO_UART_DEFAULT_CRLF PICO_STDIO_DEFAULT_CRLF
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Perform custom initialization initialize stdin/stdout over UART and add it to the current set of stdin/stdout drivers
 *  \ingroup pico_stdio_uart
 *
 * \param uart the uart instance to use, \ref uart0 or \ref uart1
 * \param baud_rate the baud rate in Hz
 * \param tx_pin the UART pin to use for stdout (or -1 for no stdout)
 * \param rx_pin the UART pin to use for stdin (or -1 for no stdin)
 */
	void stdio_async_uart_init_full(uart_inst_t * uart, uint baud_rate, int tx_pin, int rx_pin);

// Directly write to the ringbuffer, should be super fast
	void stdio_uart_out_chars(const char *buf, int length);

// Quickly print a 32-bit unsigned value as hex
	static inline void uart_print_hex_u32(uint32_t word) {
		uint8_t buf[10];

		for (int i = 0; i < 8; i++) {
			const uint8_t nibble = word >> 28;

			 buf[i] = (nibble > 9) ? 0x37 + nibble : 0x30 + nibble;

			 word <<= 4;
		} buf[8] = '\r';
		buf[9] = '\n';

		stdio_uart_out_chars(buf, sizeof(buf));
	}

#ifdef __cplusplus
}
#endif

#endif
