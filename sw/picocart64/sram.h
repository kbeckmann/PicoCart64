/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

#define SRAM_SIZE_BYTES (64 * 1024)

extern uint16_t sram[SRAM_SIZE_BYTES / sizeof(uint16_t)];
