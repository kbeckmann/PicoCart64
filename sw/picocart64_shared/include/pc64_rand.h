/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

uint32_t pc64_rand32(void);
uint16_t pc64_rand16(void);
uint8_t pc64_rand8(void);
void pc64_rand_seed(uint32_t new_seed);
