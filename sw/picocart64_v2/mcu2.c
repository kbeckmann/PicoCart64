/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"

#include "pins_mcu2.h"
#include "git_info.h"
#include "stdio_async_uart.h"

#define UART0_BAUD_RATE  (115200)

void mcu2_main(void)
{
	int count = 0;

	// Init async UART on pin 0/1
	stdio_async_uart_init_full(uart0, UART0_BAUD_RATE, PIN_UART0_TX, PIN_UART0_RX);

	printf("PicoCart64 Boot (git rev %08x)\r\n", GIT_REV);

	// TODO: Remove later
	sleep_ms(1000);

	// Boot MCU1
	gpio_set_dir(PIN_MCU1_RUN, GPIO_OUT);
	gpio_put(PIN_MCU1_RUN, 1);	// Set GPIO19 / MCU2.RUN to HIGH

	while (true) {
		count++;
		printf("Hello, world! I am MCU 2 -- %d\n", count);
		sleep_ms(1000);
	}
}
