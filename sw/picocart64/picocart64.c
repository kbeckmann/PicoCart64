/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"

#include "stdio_async_uart.h"

#include "cic.h"
#include "picocart64_pins.h"
#include "picocart64.h"
#include "utils.h"
#include "n64_pi.h"
#include "git_info.h"

#define PICO_LA1    (26)
#define PICO_LA2    (27)

#define UART_TX_PIN (28)
#define UART_RX_PIN (29)		/* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

#define ENABLE_N64_PI 1

// Priority 0 = lowest, 31 = highest
// Use same priority to force round-robin scheduling
#define CIC_TASK_PRIORITY     (tskIDLE_PRIORITY + 1UL)
#define SECOND_TASK_PRIORITY  (tskIDLE_PRIORITY + 1UL)

static StaticTask_t cic_task;
/* static */ StaticTask_t second_task;
static StackType_t cic_task_stack[configMINIMAL_STACK_SIZE];
static StackType_t second_task_stack[configMINIMAL_STACK_SIZE];

/*

Profiling results:

Time between ~N64_READ and bit output on AD0

133 MHz old code:
    ROM:  1st _980_ ns, 2nd 500 ns
    SRAM: 1st  500  ns, 2nd 510 ns

133 MHz new code:
    ROM:  1st _300_ ns, 2nd 280 ns
    SRAM: 1st  320  ns, 2nd 320 ns

266 MHz new code:
    ROM:  1st  180 ns, 2nd 180 ns (sometimes down to 160, but only worst case matters)
    SRAM: 1st  160 ns, 2nd 160 ns

*/

// FreeRTOS boilerplate
void vApplicationGetTimerTaskMemory(StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, uint32_t * pulTimerTaskStackSize)
{
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void cic_task_entry(__unused void *params)
{
	printf("cic_task_entry\n");

	cic_main();
}

#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"

#include "ping.h"

#define PING_ADDR "192.168.5.18"

void ping_task(__unused void *params)
{
	if (cyw43_arch_init()) {
		printf("failed to initialise\n");
		return;
	}
	cyw43_arch_enable_sta_mode();
	printf("Connecting to WiFi...\n");
	if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
		printf("failed to connect.\n");
		exit(1);
	} else {
		printf("Connected.\n");
	}

	ip_addr_t ping_addr;
	ip4_addr_set_u32(&ping_addr, ipaddr_addr(PING_ADDR));
	ping_init(&ping_addr);

	while (true) {
		// not much to do as LED is in another task, and we're using RAW (callback) lwIP API
		vTaskDelay(10000);
	}

	cyw43_arch_deinit();
}

void second_task_entry(__unused void *params)
{
	uint32_t count = 0;

	printf("second_task_entry\n");

	while (true) {
		vTaskDelay(1000);
		count++;

		// Set to 1 to print stack watermarks.
		// Printing is synchronous and interferes with the CIC emulation.
#if 0
		// printf("Second task heartbeat: %d\n", count);
		// vPortYield();

		if (count > 10) {
			printf("watermark: %d\n", uxTaskGetStackHighWaterMark(NULL));
			vPortYield();

			printf("watermark second_task: %d\n", uxTaskGetStackHighWaterMark((TaskHandle_t) & second_task));
			vPortYield();

			printf("watermark cic_task: %d\n", uxTaskGetStackHighWaterMark((TaskHandle_t) & cic_task));
			vPortYield();
		}
#endif

	}
}

void vLaunch(void)
{
	xTaskCreateStatic(cic_task_entry, "CICThread", configMINIMAL_STACK_SIZE, NULL, CIC_TASK_PRIORITY, cic_task_stack, &cic_task);
	// xTaskCreateStatic(second_task_entry, "SecondThread", configMINIMAL_STACK_SIZE, NULL, SECOND_TASK_PRIORITY, second_task_stack, &second_task);
	xTaskCreateStatic(ping_task, "SecondThread", configMINIMAL_STACK_SIZE, NULL, SECOND_TASK_PRIORITY, second_task_stack, &second_task);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();
}

int main(void)
{
	// Overclock!
	// The external flash should be rated to 133MHz,
	// but since it's used with a 2x clock divider,
	// 266 MHz is safe in this regard.

	// set_sys_clock_khz(133000, true);
	set_sys_clock_khz(266000, true);	// Required for SRAM @ 200ns

	// Init GPIOs before starting the second core and FreeRTOS
	for (int i = 0; i <= 27; i++) {
		gpio_init(i);
		gpio_set_dir(i, GPIO_IN);
		gpio_set_pulls(i, false, false);
	}

	// Enable pull up on N64_CIC_DIO since there is no external one.
	gpio_pull_up(N64_CIC_DIO);

	// Init UART on pin 28/29
	stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
	printf("PicoCart64 Boot (git rev %08x)\r\n", GIT_REV);

#if ENABLE_N64_PI
	// Launch the N64 PI implementation in the second core
	// Note! You have to power reset the pico after flashing it with a jlink,
	//       otherwise multicore doesn't work properly.
	//       Alternatively, attach gdb to openocd, run `mon reset halt`, `c`.
	//       It seems this works around the issue as well.
	multicore_launch_core1(n64_pi_run);
#endif

	// Start FreeRTOS on Core0
	vLaunch();

	return 0;
}
