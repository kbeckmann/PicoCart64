/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/clocks.h"

#include "FreeRTOS.h"
#include "task.h"

#include "esp32_task.h"
#include "n64_cic.h"
#include "git_info.h"
#include "led_task.h"
#include "reset_reason.h"
#include "pins_mcu2.h"
#include "utils.h"
#include "qspi_helper.h"
#include "stdio_async_uart.h"
#include "gpio_helper.h"
#include "psram_inline.h"

#include "pio_uart/pio_uart.h"

#define UART0_BAUD_RATE  (115200)

// Priority 0 = lowest, 31 = highest
// Use same priority to force round-robin scheduling
#define LED_TASK_PRIORITY     (tskIDLE_PRIORITY + 1UL)
#define ESP32_TASK_PRIORITY   (tskIDLE_PRIORITY + 1UL)
#define MAIN_TASK_PRIORITY    (tskIDLE_PRIORITY + 1UL)

static StaticTask_t main_task;
static StaticTask_t led_task;
static StaticTask_t esp32_task;

#define MAIN_TASK_STACK_SIZE (1024)
#define LED_TASK_STACK_SIZE (1024)
#define ESP32_TASK_STACK_SIZE (1024)

static StackType_t main_task_stack[MAIN_TASK_STACK_SIZE];
static StackType_t led_task_stack[LED_TASK_STACK_SIZE];
static StackType_t esp32_task_stack[ESP32_TASK_STACK_SIZE];

static const gpio_config_t mcu2_gpio_config[] = {
	{PIN_UART0_TX, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_UART},
	{PIN_UART0_RX, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_UART},

	// TODO: Configure as PIO0 probably?
	{PIN_SD_CLK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_CMD, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT0_UART1_TX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT1_UART1_RX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT3, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// TODO: Configure as PIO1 probably?
	{PIN_ESP32_D0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D3, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_CS, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_SCK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Demux should be configured as inputs without pulls until we lock the bus
	{PIN_DEMUX_A0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_IE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// MCU1 RUN/RESETn pin
	{PIN_MCU1_RUN, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// WS2812 RGB LED
	{PIN_LED, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Configure as a Clock Output clk_gpout0 after GPIO config
	{PIN_MCU2_GPIO21, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_2MA, GPIO_FUNC_SIO},

	// N64 signals
	{PIN_N64_COLD_RESET, GPIO_IN, false, false, true, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled down
	{PIN_N64_NMI, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up, open drain
	{PIN_CIC_DIO, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up
	{PIN_CIC_DCLK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Configure as PIO that implements SPI becase of the way the pins from MCU1 are connected to MCU2
	{PIN_SPI1_SCK, GPIO_IN, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1}, // used when spi slave
	// {PIN_SPI1_SCK, GPIO_OUT, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
	//{PIN_SPI1_TX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1}, // not using
	{PIN_SPI1_RX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
	{PIN_SPI1_CS, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
};

static void psram_test(void)
{
	vTaskDelay(500);

	printf("qspi_test();\n");
	qspi_test();

	// Turn off the XIP cache
	xip_ctrl_hw->ctrl = (xip_ctrl_hw->ctrl & (~XIP_CTRL_EN_BITS));
	xip_ctrl_hw->flush = 1;

	qspi_enable();
	qspi_enter_cmd_xip();

	printf("Read:\n");
	uint32_t *ptr = (uint32_t *) 0x10000000;
	// for (int i = 0; i < 8 * 1024 / 4 * 2; i++) {
	for (int i = 0; i < 256; i++) {
		uint32_t address_32 = i;
		uint32_t address = address_32 * 4;
		psram_set_cs(psram_addr_to_chip(address));
		uint32_t word1 = ptr[address_32];
		psram_set_cs(0);
		// printf("%08X ", word);
		if (word1 != i) {
			printf("ERR: %08X != %08X\n", word1, i);
		}
	}

	printf("------ Read done\n");

	qspi_disable();
}

void main_task_entry(__unused void *params)
{
	int count = 0;

	printf("[MCU2 Main] Hello\n");

	// // Test PSRAM - write test pattern and read it back.
	// psram_test();
	// while (1) {
	// 	printf("\n\n**** HALT \n");
	// 	vTaskDelay(5 * 1000);
	// }

	// vTaskDelay(1000);

	// Write 16 MB (takes a while)
	// write_random_file("random.bin", 16 * 1024 * 1024);

	// write_random_file("random.bin", 9 * 1024 * 1024);
	// load_rom("random.bin");

	//load_rom("testrom.z64");

	// while (1) {
	// 	vTaskDelay(5 * 1000);
	// }

	// Setup PIO UART
	pio_uart_inst_t uart_rx = {
            .pio = pio1,
            .sm = 0
    };

	pio_uart_inst_t uart_tx = {
            .pio = pio1,
            .sm = 1
    };
	uint pioUartRXOffset = pio_add_program(uart_rx.pio, &uart_rx_program);
	//uint pioUartTXOffset = pio_add_program(uart_tx.pio, &uart_tx_program);

	uart_rx_program_init(uart_rx.pio, uart_rx.sm, pioUartRXOffset, PIN_SPI1_CS, PIO_UART_BAUD_RATE);
	//uart_tx_program_init(uart_tx.pio, uart_tx.sm, pioUartTXOffset, PIN_SPI1_RX, PIO_UART_BAUD_RATE);

	printf("Booting MCU1...\n");
	// Boot MCU1
	gpio_put(PIN_MCU1_RUN, 1);

	printf("MCU1 should be booting...\n");

	vTaskDelay(1 * 1500);

	uint32_t BUFFER_SIZE = 8;
	uint8_t writeBuffer[BUFFER_SIZE];
	uint8_t readBuffer[BUFFER_SIZE];
	int lastRead = 0;
	while(1) {
		int now = time_us_32();
		//if (now - lastRead > 3000000) {
			printf("\nReading pio spi...\n\n");
				
			for(int i = 0; i < BUFFER_SIZE; i++) {
				char c = uart_rx_program_getc(uart_rx.pio, uart_rx.sm);
				printf("%c", c);
			}
			printf("\nFinished!\n");
			lastRead = now;
		//}
		
	}

	while (true) {
		count++;

#if 1
		printf("----------------------------------------\n");
		printf("MCU 2 [i=%d]\n", count);
		printf("Stack usage main_task: %d bytes\n", MAIN_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark(NULL));
		printf("Stack usage led_task: %d bytes\n", LED_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark((TaskHandle_t) & led_task));
		printf("Stack usage esp32_task: %d bytes\n", ESP32_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark((TaskHandle_t) & esp32_task));
		printf("----------------------------------------\n\n");
#endif

		vTaskDelay(1000);
	}
}

void mcu2_core1_entry(void)
{
	printf("[Core1] CIC Starting\n");

	while (1) {
		n64_cic_run(PIN_N64_COLD_RESET, PIN_CIC_DCLK, PIN_CIC_DIO);

		// n64_cic_run returns when N64_CR goes low, i.e.
		// user presses the reset button, or the N64 loses power.

		// TODO: Perform actions when this happens. Commit RAM to flash/SD etc.

		printf("[Core1] CIC Restarting\n");
	}
}

void vLaunch(void)
{
	xTaskCreateStatic(main_task_entry, "Main", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, main_task_stack, &main_task);
	xTaskCreateStatic(led_task_entry, "LED", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, led_task_stack, &led_task);
	xTaskCreateStatic(esp32_task_entry, "ESP32", ESP32_TASK_STACK_SIZE, NULL, ESP32_TASK_PRIORITY, esp32_task_stack, &esp32_task);

	// Start the tasks and timer.
	vTaskStartScheduler();
}

void mcu2_main(void)
{
	const int freq_khz = 133000;
	// const int freq_khz = 200000;
	// const int freq_khz = 210000;
	// const int freq_khz = 220000;
	// const int freq_khz = 230000;
	// const int freq_khz = 240000;
	// const int freq_khz = 266000;

	// Note that this might call set_sys_clock_pll,
	// which might set clk_peri to 48 MHz
	set_sys_clock_khz(freq_khz, true);

	// Init async UART on pin 0/1
	// stdio_async_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	stdio_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	// stdio_usb_init();

	gpio_configure(mcu2_gpio_config, ARRAY_SIZE(mcu2_gpio_config));

	// Enable a 12MHz clock output on GPIO21 / clk_gpout0
	clock_gpio_init(PIN_MCU2_GPIO21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);

	printf("\n\n----------------------------------------\n");
	printf("PicoCart64 MCU2 Boot (git rev %08x)\r\n", GIT_REV);
	printf("Reset reason: 0x%08lX\n", get_reset_reason());
	printf("clk_sys: %d Hz\n", clock_get_hz(clk_sys));
	printf("clk_peri: %d Hz\n", clock_get_hz(clk_peri));
	printf("----------------------------------------\n\n");

	multicore_launch_core1(mcu2_core1_entry);

	// Start FreeRTOS on Core0
	vLaunch();

	while (true) {
		// Never reached
	}
}
