/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

/*
 * This code demonstrates how to boot two RP2040 devices with a shared flash chip.
 * MCU1       <-> FLASH    <-> MCU2
 * MCU1.GPIO2  -> MCU2.RUN  -> 10k Pull Down
 * MCU2.GPIO2               -> 10k Pull Up
 *
 * MCU1 boots first, copies program from flash to RAM and executes.
 * MCU1 reads GPIO2. If LOW: we are MCU1.
 * MCU1 sets QSPI OutputEnable override to Disabled.
 * MCU1 drives GPIO2 HIGH, letting MCU2 boot.
 * MCU2 boots, copies program from flash to RAM and executes.
 * MCU2 reads GPIO2. If HIGH: we are MCU2.
 * MCU2 sets QSPI OutputEnable override to Disabled.
 * Done.
 *
 */

int main(void)
{
	int count = 0;

	// On MCU1, GPIO 2 is pulled low externally and connected to MCU2.RUN.
	// On MCU2, GPIO 2 is pulled high externally.
	gpio_init(2);
	gpio_set_dir(2, GPIO_IN);

	// GPIO 25 / LED is a slow signal to show if we are running.
	gpio_init(25);
	gpio_set_dir(25, GPIO_OUT);

	// mcu_id will be 1 for MCU1, and 2 for MCU2.
	int mcu_id = gpio_get(2) + 1;

	stdio_init_all();

	// Turn off SSI
	ssi_hw->ssienr = 0;

	// Set all QSPI IOs high impedance (disable OE)
	for (int i = 0; i < 6; i++) {
		ioqspi_hw->io[i].ctrl = (ioqspi_hw->io[i].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
								 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));
	}

	if (mcu_id == 1) {
		// I am MCU1, let MCU2 boot
		gpio_set_dir(2, GPIO_OUT);
		gpio_put(2, 1);			// Set GPIO2 / MCU2.RUN to HIGH
	}

	while (true) {
		count++;
		printf("Hello, world! I am MCU %d -- %d\n", mcu_id, count);
		gpio_put(25, count & 1);	// Toggle LED slowly
		sleep_ms(1000);
	}
	return 0;
}
