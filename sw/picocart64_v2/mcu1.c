/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"

void mcu1_main(void)
{
	int count = 0;

	// Enable STDIO over USB
	stdio_usb_init();

	while (true) {
		count++;
		printf("Hello, world! I am MCU 1 -- %d\n", count);
		sleep_ms(1000);
	}
}
