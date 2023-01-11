/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include "hardware/gpio.h"

typedef struct {
	uint8_t gpio;
	uint8_t dir;
	uint8_t value;
	uint8_t pull_up;
	uint8_t pull_down;
	uint8_t drive_strength;
	uint8_t function;
} gpio_config_t;

static inline void gpio_configure(const gpio_config_t * config, uint32_t num_config)
{
	for (int i = 0; i < num_config; i++) {
		const gpio_config_t *c = &config[i];
		// printf("GPIO %d %d %d %d %d %d %d\n", c->gpio, c->dir, c->value, c->pull_up, c->pull_down, c->drive_strength, c->function);

		gpio_set_pulls(c->gpio, c->pull_up, c->pull_down);
		gpio_set_drive_strength(c->gpio, c->drive_strength);

		if (c->function == GPIO_FUNC_SIO) {
			gpio_init(c->gpio);
			gpio_set_dir(c->gpio, c->dir);
			if (c->dir == GPIO_OUT) {
				gpio_put(c->gpio, c->value);
			}
		} else {
			gpio_set_function(c->gpio, c->function);
		}
	}
}
