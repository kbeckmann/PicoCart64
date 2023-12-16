/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

/**
 * Generate a random 32-bit integer.
 *
 * This function generates a random 32-bit integer using a linear congruential generator.
 *
 * @return A random 32-bit integer.
 */
uint32_t pc64_rand32(void);

/**
 * Generate a random 16-bit integer.
 *
 * This function generates a random 16-bit integer by calling pc64_rand32 and truncating the result.
 *
 * @return A random 16-bit integer.
 */
uint16_t pc64_rand16(void);

/**
 * Generate a random 8-bit integer.
 *
 * This function generates a random 8-bit integer by calling pc64_rand32 and truncating the result.
 *
 * @return A random 8-bit integer.
 */
uint8_t pc64_rand8(void);

/**
 * Seed the random number generator.
 *
 * This function sets the seed used by the random number generator.
 *
 * @param new_seed The new seed.
 */
void pc64_rand_seed(uint32_t new_seed);
