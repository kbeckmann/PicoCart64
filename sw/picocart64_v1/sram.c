/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <string.h>
#include <stdint.h>

#include "sram.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"

// uint16_t sram[SRAM_1MBIT_SIZE / sizeof(uint16_t)];
uint16_t sram[1024 / sizeof(uint16_t)];

void sram_load_from_flash(void)
{
	// Not supported anymore
}

void sram_save_to_flash(void)
{
	// Not supported anymore
}
