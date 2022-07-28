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

uint16_t sram[SRAM_1MBIT_SIZE / sizeof(uint16_t)];

// sram_backup will be aligned to 4096 to match the flash sector erase size
uint16_t __aligned(4096) __attribute__((section(".n64_sram"))) sram_backup[SRAM_1MBIT_SIZE / sizeof(uint16_t)];

void sram_load_from_flash(void)
{
	memcpy(sram, sram_backup, sizeof(sram));
}

void sram_save_to_flash(void)
{
	uint32_t offset = ((uint32_t) sram_backup) - XIP_BASE;
	uint32_t count = sizeof(sram);

	flash_range_erase(offset, count);
	flash_range_program(offset, (const uint8_t *)sram, count);
}
