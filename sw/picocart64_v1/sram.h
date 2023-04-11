/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

#define SRAM_FB_SIZE_16 (320 * 240)
// #define SRAM_FB_SIZE_16 (320 * 180)

extern uint16_t sram[SRAM_FB_SIZE_16];

void sram_load_from_flash(void);
void sram_save_to_flash(void);
