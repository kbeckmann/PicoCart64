/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pins_mcu2.h"
#include "git_info.h"
#include "stdio_async_uart.h"
#include "led_task.h"

#define UART0_BAUD_RATE  (115200)

// Priority 0 = lowest, 31 = highest
// Use same priority to force round-robin scheduling
#define LED_TASK_PRIORITY     (tskIDLE_PRIORITY + 1UL)
#define MISC_TASK_PRIORITY    (tskIDLE_PRIORITY + 1UL)

static StaticTask_t led_task;
static StaticTask_t misc_task;

static StackType_t led_task_stack[configMINIMAL_STACK_SIZE];
static StackType_t misc_task_stack[configMINIMAL_STACK_SIZE];

void misc_task_entry(__unused void *params)
{
	int count = 0;

	while (true) {
		count++;
		printf("Hello, world! I am MCU 2 -- %d\n", count);
		vTaskDelay(1000);
	}
}

void vLaunch(void)
{
	xTaskCreateStatic(led_task_entry, "LED", configMINIMAL_STACK_SIZE, NULL, LED_TASK_PRIORITY, led_task_stack, &led_task);
	xTaskCreateStatic(misc_task_entry, "Misc", configMINIMAL_STACK_SIZE, NULL, MISC_TASK_PRIORITY, misc_task_stack, &misc_task);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();
}

void mcu2_main(void)
{
	// Init async UART on pin 0/1
	stdio_async_uart_init_full(uart0, UART0_BAUD_RATE, PIN_UART0_TX, PIN_UART0_RX);

	printf("PicoCart64 Boot (git rev %08x)\r\n", GIT_REV);

	// Boot MCU1
	gpio_set_dir(PIN_MCU1_RUN, GPIO_OUT);
	gpio_put(PIN_MCU1_RUN, 1);	// Set GPIO19 / MCU2.RUN to HIGH

	// TODO: Launch CIC emulation on the second core
	// multicore_launch_core1(n64_cic);

	// Start FreeRTOS on Core0
	vLaunch();

	while (true) {
		// Never reached
	}
}
