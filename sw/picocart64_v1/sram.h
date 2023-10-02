/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

#define SRAM_FB_SIZE_16 (320 * 240)

extern uint16_t sram[SRAM_FB_SIZE_16];

static inline uint32_t sram_resolve_address_shifted(uint32_t pi_address)
{
	// Banked mode disabled

	uint32_t resolved_address;
	resolved_address = pi_address & (sizeof(sram) - 1);
	return resolved_address >> 1;
}

void sram_load_from_flash(void);
void sram_save_to_flash(void);
