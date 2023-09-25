/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

#define SRAM_256KBIT_SIZE         0x00008000
#define SRAM_768KBIT_SIZE         0x00018000
#define SRAM_1MBIT_SIZE           0x00020000

/* SRAM
 * 
 * 256 kbit = 32kByte = 32768 bytes
 * Bits [14:0] are used to address this space, i.e. 15 active bits:
 *   0b00000000_00000000_01111111_11111111
 * 
 * 1MBit contiguous mode expands this to [16:0], i.e. 17 active bits:
 *   0b00000000_00000001_11111111_11111111
 *
 * Banked mode uses bits [19:18] to select up to 4 banks of 256kbit each.
 *   0b00000000_0000xx00_01111111_11111111
 *   0b00000000_00001100_00000000_00000000
 *                  ^^
 * 
 * In our implementation, we can support both contiguous and banked mode.
 * 
 */

extern uint16_t sram[SRAM_1MBIT_SIZE / sizeof(uint16_t)];

static inline uint32_t sram_resolve_address_shifted(uint32_t pi_address)
{
#if 0
	// Banked mode, readable code

	uint32_t bank = (pi_address >> 18) & 0x3;
	uint32_t resolved_address;

	resolved_address = pi_address & (sizeof(sram) - 1);
	resolved_address |= bank << 15;

	return resolved_address >> 1;

#elif 0
	// Banked mode disabled

	uint32_t resolved_address;
	resolved_address = pi_address & (sizeof(sram) - 1);
	return resolved_address >> 1;
#else
	// Banked mode, faster implementation
	return (((pi_address >> 18) << 30) | (pi_address << 15)) >> 16;
#endif
}

void sram_load_from_flash(void);
void sram_save_to_flash(void);
