/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

/// @brief Macro to get the size of an array
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

/**
 * @brief Swap the lower and upper 16 bits of a 32-bit value
 * 
 * @param value The 32-bit value to swap
 * @return The 32-bit value with the lower and upper 16 bits swapped
 * 
 * Example: 0x11223344 => 0x33441122
 */
static inline uint32_t swap16(uint32_t value)
{
	return (value << 16) | (value >> 16);
}

/**
 * @brief Swap the lower and upper 8 bits of a 16-bit value
 * 
 * @param value The 16-bit value to swap
 * @return The 16-bit value with the lower and upper 8 bits swapped
 * 
 * Example: 0x1122 => 0x2211
 */
static inline uint16_t swap8(uint16_t value)
{
	return (value << 8) | (value >> 8);
}

/**
 * @brief Handler for assertion failures
 * 
 * @param file The file where the assertion failed
 * @param line The line number where the assertion failed
 * @param statement The assertion statement that failed
 */
void assert_handler(char *file, int line, char *statement);

/**
 * @brief Macro for assertions
 * 
 * @param _stmt The assertion statement
 * 
 * If the assertion statement is false, the assert_handler is called with the file, line, and statement.
 */
#define ASSERT(_stmt) do                                                      \
{                                                                             \
	if (!(_stmt)) {                                                           \
		assert_handler(__FILE__, __LINE__, #_stmt);                           \
	}                                                                         \
} while (0)
