/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdint.h>

#include "ws2812.h"
#include "hardware/pio.h"

#include "ws2812.pio.h"

static struct {
	bool initialized;
	PIO pio;
	uint8_t gpio;
} self;

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t) (r) << 16) | ((uint32_t) (g) << 24) | ((uint32_t) (b) << 8);
}

void ws2812_init(PIO pio, uint8_t gpio)
{
	if (self.initialized) {
		return;
	}

	uint offset = pio_add_program(pio, &ws2812_program);
	ws2812_program_init(pio, 0, offset, gpio, 800000, false);

	self.pio = pio;
	self.gpio = gpio;
	self.initialized = true;
}

void ws2812_write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if (!self.initialized) {
		return;
	}

	pio_sm_put_blocking(self.pio, 0, urgb_u32(r, g, b));
}
