#include <stdint.h>

#include "ws2812b.h"

void ws2812b_write(int gpio, const uint8_t * pixels, uint32_t num_bytes)
{
	// Either use PIO based on https://github.com/adafruit/circuitpython/blob/main/ports/raspberrypi/common-hal/neopixel_write/__init__.c
	// or bitbang?
}

void ws2812b_write_rgb(int gpio, uint8_t r, uint8_t g, uint8_t b)
{
	const uint8_t rgb[] = {
		r,
		g,
		b,
	};

	ws2812b_write(gpio, rgb, sizeof(rgb));
}
