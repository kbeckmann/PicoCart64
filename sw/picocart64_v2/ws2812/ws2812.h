/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include "hardware/pio.h"

#include <stdint.h>

void ws2812_init(PIO pio, uint8_t gpio);
void ws2812_write_rgb(uint8_t r, uint8_t g, uint8_t b);
