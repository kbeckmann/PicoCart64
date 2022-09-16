/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static inline uint32_t swap16(uint32_t value)
{
	// 0x11223344 => 0x33441122
	return (value << 16) | (value >> 16);
}

static inline uint16_t swap8(uint16_t value)
{
	// 0x1122 => 0x2211
	return (value << 8) | (value >> 8);
}

void assert_handler(char *file, int line, char *statement);

#define ASSERT(_stmt) do                                                      \
{                                                                             \
    if (!(_stmt)) {                                                           \
        assert_handler(__FILE__, __LINE__, #_stmt);                           \
    }                                                                         \
} while (0)

#define LIKELY(x)    __builtin_expect ((x), 1)
#define UNLIKELY(x)  __builtin_expect ((x), 0)
