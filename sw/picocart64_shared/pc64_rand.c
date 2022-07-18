/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include "pc64_rand.h"

static uint32_t seed;

uint32_t pc64_rand32(void)
{
    seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
    return seed;
}

uint16_t pc64_rand16(void)
{
    return pc64_rand32() & 0xffff;
}

uint8_t pc64_rand8(void)
{
    return pc64_rand32() & 0xff;
}

void pc64_rand_seed(uint32_t new_seed)
{
    seed = new_seed;
}
