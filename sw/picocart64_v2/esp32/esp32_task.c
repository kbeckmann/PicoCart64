/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pins_mcu2.h"
#include "esp32_task.h"
#include "utils.h"

#define UART_ID   uart1
#define UART_BAUD_RATE 115200
#define UART_DATA_BITS 8
#define UART_STOP_BITS 1
#define UART_PARITY    UART_PARITY_NONE

#define SPI_FREQ_HZ    (10 * 1000000)

typedef struct {
	uint8_t gpio;
	uint8_t dir;
	uint8_t value;
} gpio_config_t;

static const gpio_config_t gpio_config[] = {
	{PIN_ESP32_D0, GPIO_OUT, 0},
	{PIN_ESP32_D1, GPIO_OUT, 0},
	{PIN_ESP32_D2, GPIO_OUT, 0},
	{PIN_ESP32_D3, GPIO_OUT, 0},
	{PIN_ESP32_CS, GPIO_OUT, 0},
	{PIN_ESP32_SCK, GPIO_OUT, 0},
	{PIN_ESP32_EN, GPIO_OUT, 0},
};

static struct {
	uint32_t fw_version;
} self;

static void esp32_gpio_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(gpio_config); i++) {
		gpio_init(gpio_config[i].gpio);
		gpio_set_dir(gpio_config[i].gpio, gpio_config[i].dir);
		if (gpio_config[i].dir == GPIO_OUT) {
			gpio_put(gpio_config[i].gpio, gpio_config[i].value);
		}
	}
}

static void esp32_set_boot_straps(bool bootloader)
{
	// Table 3 from https://www.espressif.com/sites/default/files/documentation/esp32-c3-wroom-02_datasheet_en.pdf

	if (bootloader) {
		// Set pins so we enter bootloader mode
		gpio_put(PIN_ESP32_CS, 1);	// ESP32_GPIO2
		gpio_put(PIN_ESP32_SCK, 1);	// ESP32_GPIO8
		gpio_put(PIN_ESP32_D2, 0);	// ESP32_GPIO9
	} else {
		// Set pins so we boot from flash
		gpio_put(PIN_ESP32_CS, 1);	// ESP32_GPIO2
		gpio_put(PIN_ESP32_SCK, 1);	// ESP32_GPIO8
		gpio_put(PIN_ESP32_D2, 1);	// ESP32_GPIO9
	}
}

static void esp32_set_en(bool enabled)
{
	gpio_put(PIN_ESP32_EN, enabled);
}

static void esp32_uart_init(void)
{
	uart_init(UART_ID, UART_BAUD_RATE);
	gpio_set_function(PIN_ESP32_D0, GPIO_FUNC_UART);	// ESP32_GPIO20/RX
	gpio_set_function(PIN_ESP32_D1, GPIO_FUNC_UART);	// ESP32_GPIO21/TX
	uart_set_hw_flow(UART_ID, false, false);
	uart_set_format(UART_ID, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
	uart_set_fifo_enabled(UART_ID, false);	// or true?
}

void esp32_task_entry(__unused void *params)
{
	esp32_gpio_init();
	esp32_uart_init();

	vTaskDelay(100);

	// Set boot straps to spi boot and boot ESP32
	// esp32_set_boot_straps(false);
	esp32_set_boot_straps(true);
	esp32_set_en(true);

	// TODO: Check version with SPI
	// If version does not match, enter bootloader mode and program it.

	// $ esptool.py --port /dev/ttyUSB0 --baud 115200 --chip esp32c3 --no-stub write_flash 0x0000 hello_world.bin

	while (true) {
		// TODO: This is blocking and bad, but ok for bringup
		while (uart_is_readable(UART_ID)) {
			uint8_t ch = uart_getc(UART_ID);
			uart_putc(uart0, ch);
			// printf("%c", ch);
		}

		// Read from debug uart and forward to esp32
		while (uart_is_readable(uart0)) {
			uint8_t ch = uart_getc(uart0);
			// printf("%c", ch);
			uart_putc(UART_ID, ch);
		}

		taskYIELD();
	}
}
