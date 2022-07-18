/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

// PicoCart64 Address space

// [READ/WRITE]: Scratch memory used for various functions
#define PC64_BASE_ADDRESS_START    0x81000000
#define PC64_BASE_ADDRESS_LENGTH   0x00001000
#define PC64_BASE_ADDRESS_END      (PC64_BASE_ADDRESS_START + PC64_BASE_ADDRESS_LENGTH - 1)

// [READ]: Returns pseudo-random values.
//         Address does not matter.
//         Each returned 16-bit word generates a new random value.
//         PC64_REGISTER_RESET_RAND resets the random seed.
#define PC64_RAND_ADDRESS_START    0x82000000
#define PC64_RAND_ADDRESS_LENGTH   0x01000000
#define PC64_RAND_ADDRESS_END      (PC64_RAND_ADDRESS_START + PC64_RAND_ADDRESS_LENGTH - 1)

// [READ/WRITE]: Command address space. See register definitions below for details.
#define PC64_CIBASE_ADDRESS_START  0x83000000
#define PC64_CIBASE_ADDRESS_LENGTH 0x00001000
#define PC64_CIBASE_ADDRESS_END    (PC64_CIBASE_ADDRESS_START + PC64_CIBASE_ADDRESS_LENGTH - 1)

// [Read]: Returns PC64_MAGIC
#define PC64_REGISTER_MAGIC        0x00000000
#define PC64_MAGIC                 0xDEAD6400

// [WRITE]: Write number of bytes to print from TX buffer
#define PC64_REGISTER_UART_TX      0x00000004

// [WRITE]: Set the random seed to a 32-bit value
#define PC64_REGISTER_RAND_SEED    0x00000008
