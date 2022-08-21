/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "FreeRTOS.h"
#include "task.h"

#include "esp32_task.h"
#include "n64_cic.h"
#include "git_info.h"
#include "led_task.h"
#include "pins_mcu2.h"
#include "stdio_async_uart.h"

#define UART0_BAUD_RATE  (115200)

// Priority 0 = lowest, 31 = highest
// Use same priority to force round-robin scheduling
#define LED_TASK_PRIORITY     (tskIDLE_PRIORITY + 1UL)
#define ESP32_TASK_PRIORITY   (tskIDLE_PRIORITY + 1UL)
#define MISC_TASK_PRIORITY    (tskIDLE_PRIORITY + 1UL)

static StaticTask_t led_task;
static StaticTask_t esp32_task;
static StaticTask_t misc_task;

static StackType_t led_task_stack[configMINIMAL_STACK_SIZE];
static StackType_t esp32_task_stack[configMINIMAL_STACK_SIZE];
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

void mcu2_core1_entry(void)
{
	while (1) {
		n64_cic_run(PIN_N64_COLD_RESET, PIN_CIC_DCLK, PIN_CIC_DIO);

		// n64_cic_run returns when N64_CR goes low, i.e.
		// user presses the reset button, or the N64 loses power.

		// TODO: Perform actions when this happens. Commit RAM to flash/SD etc.

		printf("CIC emulation restarting\n");
	}
}

void vLaunch(void)
{
	xTaskCreateStatic(led_task_entry, "LED", configMINIMAL_STACK_SIZE, NULL, LED_TASK_PRIORITY, led_task_stack, &led_task);
	xTaskCreateStatic(esp32_task_entry, "ESP32", configMINIMAL_STACK_SIZE, NULL, ESP32_TASK_PRIORITY, esp32_task_stack, &esp32_task);
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
	gpio_init(PIN_MCU1_RUN);
	gpio_set_dir(PIN_MCU1_RUN, GPIO_OUT);
	gpio_put(PIN_MCU1_RUN, 1);	// Set GPIO19 / MCU2.RUN to HIGH

	multicore_launch_core1(mcu2_core1_entry);

	// Start FreeRTOS on Core0
	vLaunch();

	while (true) {
		// Never reached
	}
}
