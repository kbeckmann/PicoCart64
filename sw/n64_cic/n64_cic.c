// Original code sourced from https://github.com/Polprzewodnikowy/SummerCart64/blob/main/sw/controller/src/cic.c
// which is based on https://github.com/jago85/UltraCIC_C
// This file ports that code to FreeRTOS and integrates with the PicoCart64 codebase.

// MIT License

// Copyright (c) 2019 Jan Goldacker
// Copyright (c) 2022 Mateusz Faderewski
// Copyright (c) 2023 Konrad Beckmann

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "n64_cic.h"
#include "sram.h"
#include "picocart64_pins.h"

typedef enum {
	REGION_NTSC,
	REGION_PAL,
	__REGION_MAX
} cic_region_t;

#if CONFIG_REGION_NTSC == 1
#define GET_REGION() (REGION_NTSC)
#elif CONFIG_REGION_PAL == 1
#define GET_REGION() (REGION_PAL)
#else
#error Please pass -DREGION=PAL or NTSC to your cmake command line.
#endif

static volatile bool cic_enabled = false;
static volatile bool cic_detect_enabled;

static volatile uint8_t cic_next_rd;
static volatile uint8_t cic_next_wr;

static volatile bool cic_disabled = false;
static volatile bool cic_dd_mode = false;
static volatile uint8_t cic_seed = 0x3F;
static volatile uint8_t cic_checksum[6] = { 0xA5, 0x36, 0xC0, 0xF1, 0xD8, 0x59 };

SemaphoreHandle_t xSemaphore = NULL;
StaticSemaphore_t xSemaphoreBuffer;

static uint8_t cic_ram[32];
static uint8_t cic_x105_ram[30];

static const uint8_t cic_ram_init[2][32] = { {
											  0x0E, 0x00, 0x09, 0x0A, 0x01, 0x08, 0x05, 0x0A, 0x01, 0x03, 0x0E, 0x01, 0x00, 0x0D, 0x0E, 0x0C,
											  0x00, 0x0B, 0x01, 0x04, 0x0F, 0x08, 0x0B, 0x05, 0x07, 0x0C, 0x0D, 0x06, 0x01, 0x0E, 0x09, 0x08}, {
																																				0x0E, 0x00, 0x04, 0x0F, 0x05, 0x01,
																																				0x02, 0x01, 0x07, 0x01, 0x09, 0x08,
																																				0x05, 0x07, 0x05, 0x0A,
																																				0x00, 0x0B, 0x01, 0x02, 0x03, 0x0F,
																																				0x08, 0x02, 0x07, 0x01, 0x09, 0x08,
																																				0x01, 0x01, 0x05, 0x0C}
};

static void cic_irq_reset_falling(void)
{
	cic_enabled = false;
	gpio_set_dir(N64_CIC_DIO, GPIO_IN);
	// led_clear_error(LED_ERROR_CIC);
}

static void cic_irq_reset_rising(void)
{
	if (!cic_disabled) {
		cic_enabled = true;
	}
}

static void cic_irq_reset(uint gpio, uint32_t event_mask)
{
	if (gpio_get(N64_COLD_RESET)) {
		cic_irq_reset_rising();
	} else {
		cic_irq_reset_falling();
	}
}

static void cic_irq_clk_falling(void)
{
	if (cic_enabled) {
		if (!cic_next_wr) {
			gpio_put(N64_CIC_DIO, 0);
			gpio_set_dir(N64_CIC_DIO, GPIO_OUT);
		}
		cic_next_rd = gpio_get(N64_CIC_DIO) ? 1 : 0;

		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void cic_irq_clk_rising(void)
{
	gpio_set_dir(N64_CIC_DIO, GPIO_IN);
	if (cic_detect_enabled) {
		cic_detect_enabled = false;
		if (!gpio_get(N64_CIC_DIO)) {
			cic_enabled = false;
		}
	}
}

static void cic_irq_clk(uint gpio, uint32_t event_mask)
{
	if (gpio_get(N64_CIC_DCLK)) {
		cic_irq_clk_rising();
	} else {
		cic_irq_clk_falling();
	}
}

static uint8_t cic_read(void)
{
	cic_next_wr = 1;
	xSemaphoreTake(xSemaphore, portMAX_DELAY);
	return cic_next_rd;
}

static void cic_write(uint8_t bit)
{
	cic_next_wr = bit;
	xSemaphoreTake(xSemaphore, portMAX_DELAY);
}

static void cic_start_detect(void)
{
	cic_detect_enabled = cic_dd_mode;
}

static uint8_t cic_read_nibble(void)
{
	uint8_t data = 0;
	for (int i = 0; i < 4; i++) {
		data = ((data << 1) | cic_read());
	}
	return data;
}

static void cic_write_nibble(uint8_t data)
{
	cic_write(data & 0x08);
	cic_write(data & 0x04);
	cic_write(data & 0x02);
	cic_write(data & 0x01);
}

static void cic_write_ram_nibbles(uint8_t index)
{
	do {
		cic_write_nibble(cic_ram[index++]);
	} while ((index & 0x0F) != 0);
}

static void cic_encode_round(uint8_t index)
{
	uint8_t data = cic_ram[index++];
	do {
		data = ((((data + 1) & 0x0F) + cic_ram[index]) & 0x0F);
		cic_ram[index++] = data;
	} while ((index & 0x0F) != 0);
}

static void cic_write_id(cic_region_t region)
{
	cic_start_detect();
	cic_write(cic_dd_mode ? 1 : 0);
	cic_write(region == REGION_PAL ? 1 : 0);
	cic_write(0);
	cic_write(1);
}

static void cic_write_id_failed(void)
{
	// uint8_t current_region = rtc_get_region();
	// uint8_t next_region = (current_region == REGION_NTSC) ? REGION_PAL : REGION_NTSC;
	// rtc_set_region(next_region);
	// led_blink_error(LED_ERROR_CIC);
}

static void cic_write_seed(void)
{
	cic_ram[0x0A] = 0x0B;
	cic_ram[0x0B] = 0x05;
	cic_ram[0x0C] = (cic_seed >> 4);
	cic_ram[0x0D] = cic_seed;
	cic_ram[0x0E] = (cic_seed >> 4);
	cic_ram[0x0F] = cic_seed;
	cic_encode_round(0x0A);
	cic_encode_round(0x0A);
	cic_write_ram_nibbles(0x0A);
}

static void cic_write_checksum(void)
{
	for (int i = 0; i < 4; i++) {
		cic_ram[i] = 0x00;
	}
	for (int i = 0; i < 6; i++) {
		cic_ram[(i * 2) + 4] = ((cic_checksum[i] >> 4) & 0x0F);
		cic_ram[(i * 2) + 5] = (cic_checksum[i] & 0x0F);
	}
	cic_encode_round(0x00);
	cic_encode_round(0x00);
	cic_encode_round(0x00);
	cic_encode_round(0x00);
	cic_write(0);
	cic_write_ram_nibbles(0x00);
}

static void cic_init_ram(cic_region_t region)
{
	if (region < __REGION_MAX) {
		for (int i = 0; i < 32; i++) {
			cic_ram[i] = cic_ram_init[region][i];
		}
	}
	cic_ram[0x01] = cic_read_nibble();
	cic_ram[0x11] = cic_read_nibble();
}

static void cic_exchange_bytes(uint8_t * a, uint8_t * b)
{
	uint8_t tmp = *a;
	*a = *b;
	*b = tmp;
}

static void cic_round(uint8_t * m)
{
	uint8_t a, b, x;

	x = m[15];
	a = x;

	do {
		b = 1;
		a += (m[b] + 1);
		m[b] = a;
		b++;
		a += (m[b] + 1);
		cic_exchange_bytes(&a, &m[b]);
		m[b] = ~(m[b]);
		b++;
		a &= 0x0F;
		a += ((m[b] & 0x0F) + 1);
		if (a < 16) {
			cic_exchange_bytes(&a, &m[b]);
			b++;
		}
		a += m[b];
		m[b] = a;
		b++;
		a += m[b];
		cic_exchange_bytes(&a, &m[b]);
		b++;
		a &= 0x0F;
		a += 8;
		if (a < 16) {
			a += m[b];
		}
		cic_exchange_bytes(&a, &m[b]);
		b++;
		do {
			a += (m[b] + 1);
			m[b] = a;
			b++;
			b &= 0x0F;
		} while (b != 0);
		a = (x + 0x0F);
		x = (a & 0x0F);
	} while (x != 0x0F);
}

static void cic_compare_mode(cic_region_t region)
{
	cic_round(&cic_ram[0x10]);
	cic_round(&cic_ram[0x10]);
	cic_round(&cic_ram[0x10]);

	uint8_t index = (cic_ram[0x17] & 0x0F);
	if (index == 0) {
		index = 1;
	}
	index |= 0x10;

	do {
		cic_read();
		cic_write(cic_ram[index] & 0x01);
		if (region == REGION_PAL) {
			index--;
		} else {
			index++;
		}
	} while (index & 0x0F);
}

static void cic_x105_algorithm(void)
{
	uint8_t a = 5;
	uint8_t carry = 1;

	for (int i = 0; i < 30; ++i) {
		if (!(cic_x105_ram[i] & 0x01)) {
			a += 8;
		}
		if (!(a & 0x02)) {
			a += 4;
		}
		a = ((a + cic_x105_ram[i]) & 0x0F);
		cic_x105_ram[i] = a;
		if (!carry) {
			a += 7;
		}
		a = ((a + cic_x105_ram[i]) & 0x0F);
		a = (a + cic_x105_ram[i] + carry);
		if (a >= 0x10) {
			carry = 1;
			a -= 0x10;
		} else {
			carry = 0;
		}
		a = (~(a) & 0x0F);
		cic_x105_ram[i] = a;
	}
}

static void cic_x105_mode(void)
{
	cic_write_nibble(0x0A);
	cic_write_nibble(0x0A);

	for (int i = 0; i < 30; i++) {
		cic_x105_ram[i] = cic_read_nibble();
	}

	cic_x105_algorithm();

	cic_write(0);

	for (int i = 0; i < 30; i++) {
		cic_write_nibble(cic_x105_ram[i]);
	}
}

static void cic_soft_reset(cic_reset_cb_t cic_reset_cb)
{
	uint32_t t0 = xTaskGetTickCount();

	printf("[cic_soft_reset]\n");

	cic_read();

	// Notify external listener
	if (cic_reset_cb) {
		cic_reset_cb();
	}
	// Delay up to 1000ms is possible, but 500ms seems to be the norm.
	// https://n64brew.dev/wiki/PIF-NUS#Console_Reset
	// vTaskDelay(pdMS_TO_TICKS(500));

	gpio_put(N64_CIC_DIO, 0);
	gpio_set_dir(N64_CIC_DIO, GPIO_OUT);

	uint32_t t1 = xTaskGetTickCount();

	printf("[cic_soft_reset done took=%d ms]\n", t1 - t0);
}

void n64_cic_reset_parameters(void)
{
	cic_disabled = false;
	cic_dd_mode = false;
	cic_seed = 0x3F;
	cic_checksum[0] = 0xA5;
	cic_checksum[1] = 0x36;
	cic_checksum[2] = 0xC0;
	cic_checksum[3] = 0xF1;
	cic_checksum[4] = 0xD8;
	cic_checksum[5] = 0x59;
}

void n64_cic_set_parameters(uint32_t * args)
{
	cic_disabled = (args[0] >> 24) & (1 << 0);
	cic_seed = (args[0] >> 16) & 0xFF;
	cic_checksum[0] = (args[0] >> 8) & 0xFF;
	cic_checksum[1] = args[0] & 0xFF;
	cic_checksum[2] = (args[1] >> 24) & 0xFF;
	cic_checksum[3] = (args[1] >> 16) & 0xFF;
	cic_checksum[4] = (args[1] >> 8) & 0xFF;
	cic_checksum[5] = args[1] & 0xFF;
}

void n64_cic_set_dd_mode(bool enabled)
{
	cic_dd_mode = enabled;
}

void n64_cic_hw_init(void)
{
	gpio_set_irq_enabled_with_callback(N64_COLD_RESET, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &cic_irq_reset);
	gpio_set_irq_enabled_with_callback(N64_CIC_DCLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &cic_irq_clk);
}

void n64_cic_task(cic_reset_cb_t cic_reset_cb)
{
	xSemaphore = xSemaphoreCreateBinaryStatic(&xSemaphoreBuffer);

	gpio_set_dir(N64_CIC_DIO, GPIO_IN);

	while (!gpio_get(N64_COLD_RESET)) {
		vPortYield();
	}
	// printf("#");

	// cic_irq_reset(0, 0);
	cic_enabled = true;

	// printf("A");

	cic_region_t region = GET_REGION();

	// TODO: Implement soft region switching
	// cic_region_t region = rtc_get_region();
	// if (region >= __REGION_MAX) {
	//  region = REGION_NTSC;
	//  rtc_set_region(region);
	// }

	cic_write_id(region);

	// hw_tim_setup(TIM_ID_CIC, 500, cic_write_id_failed);
	cic_write_seed();
	// hw_tim_stop(TIM_ID_CIC);

	cic_write_checksum();
	cic_init_ram(region);

	while (1) {
		uint8_t cmd = 0;
		cmd |= (cic_read() << 1);
		cmd |= cic_read();

		switch (cmd) {
		case 0:{
				cic_compare_mode(region);
				break;
			}

		case 2:{
				cic_x105_mode();
				break;
			}

		case 3:{
				cic_soft_reset(cic_reset_cb);
				break;
			}

		case 1:
		default:{
				while (1) {
					vPortYield();
				}
				break;
			}
		}
	}
}
