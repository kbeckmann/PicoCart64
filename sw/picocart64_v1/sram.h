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

extern uint16_t sram[SRAM_1MBIT_SIZE / sizeof(uint16_t)];

void sram_load_from_flash(void);
void sram_save_to_flash(void);
