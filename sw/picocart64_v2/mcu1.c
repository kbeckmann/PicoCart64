/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pins_mcu1.h"

void mcu1_main(void)
{
	int count = 0;

	// Enable STDIO over USB
	// stdio_usb_init();
	stdio_async_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);

	while (true) {
		count++;
		printf("Hello, world! I am MCU 1 -- %d\n", count);
		sleep_ms(1000);
	}
}
