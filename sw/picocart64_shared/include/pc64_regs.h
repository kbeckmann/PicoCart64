/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

// PicoCart64 Address space

// [READ/WRITE]: Scratch memory used for various functions
// #define PC64_BASE_ADDRESS_START     0x81000000
#define PC64_BASE_ADDRESS_START     (0x1FFE0000) //0x20000000 - 
#define PC64_BASE_ADDRESS_LENGTH    (0x0000800)//(0x00001000)
#define PC64_BASE_ADDRESS_END       (PC64_BASE_ADDRESS_START + PC64_BASE_ADDRESS_LENGTH - 1)

// [READ]: Returns pseudo-random values.
//         Address does not matter.
//         Each returned 16-bit word generates a new random value.
//         PC64_REGISTER_RESET_RAND resets the random seed.
#define PC64_RAND_ADDRESS_START     0x82000000
#define PC64_RAND_ADDRESS_LENGTH    0x01000000
#define PC64_RAND_ADDRESS_END       (PC64_RAND_ADDRESS_START + PC64_RAND_ADDRESS_LENGTH - 1)

// [READ/WRITE]: Command address space. See register definitions below for details.
#define PC64_CIBASE_ADDRESS_START   (PC64_BASE_ADDRESS_END + 1)
#define PC64_CIBASE_ADDRESS_LENGTH  0x00000800
#define PC64_CIBASE_ADDRESS_END     (PC64_CIBASE_ADDRESS_START + PC64_CIBASE_ADDRESS_LENGTH - 1)

// [Read]: Returns PC64_MAGIC
#define PC64_REGISTER_MAGIC         0x00000000
#define PC64_MAGIC                  0xDEAD6400

// [WRITE]: Write number of bytes to print from TX buffer
#define PC64_REGISTER_UART_TX       0x00000004

// [WRITE]: Set the random seed to a 32-bit value
#define PC64_REGISTER_RAND_SEED     0x00000008

/* *** SD CARD *** */
// [READ]: Signals pico to start data read from SD Card
#define PC64_COMMAND_SD_READ       (PC64_REGISTER_RAND_SEED + 0x4)

// [READ]: Load selected rom into memory and boot, 
#define PC64_COMMAND_SD_ROM_SELECT (PC64_COMMAND_SD_READ + 0x4)

// [READ] 1 while sd card is busy, 0 once the CI is free
#define PC64_REGISTER_SD_BUSY (PC64_COMMAND_SD_ROM_SELECT + 0x4)

// [WRITE] Sector to read from SD Card, 8 bytes
#define PC64_REGISTER_SD_READ_SECTOR0 (PC64_REGISTER_SD_BUSY + 0x4)
#define PC64_REGISTER_SD_READ_SECTOR1 (PC64_REGISTER_SD_READ_SECTOR0 + 0x4)

// [WRITE] number of sectors to read from the sd card, 4 bytes
#define PC64_REGISTER_SD_READ_NUM_SECTORS (PC64_REGISTER_SD_READ_SECTOR1 + 0x4)

// [WRITE] write the selected file name that should be loaded into memory
// 255 bytes
#define PC64_REGISTER_SD_SELECT_ROM PC64_REGISTER_SD_READ_NUM_SECTORS + 0x4
