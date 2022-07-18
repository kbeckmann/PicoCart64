/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

// N64DD control registers
#define CART_DOM2_ADDR1_START     0x05000000
#define CART_DOM2_ADDR1_END       0x05FFFFFF

// N64DD IPL ROM
#define CART_DOM1_ADDR1_START     0x06000000
#define CART_DOM1_ADDR1_END       0x07FFFFFF

// Cartridge SRAM
#define CART_DOM2_ADDR2_START     0x08000000
#define CART_DOM2_ADDR2_END       0x0FFFFFFF

// Define the highest supported banked SRAM addressees
#define CART_SRAM_START           CART_DOM2_ADDR2_START
#define CART_SRAM_END             (CART_DOM2_ADDR2_START + 0x100000 - 1)

// Cartridge ROM
#define CART_DOM1_ADDR2_START     0x10000000
#define CART_DOM1_ADDR2_END       0x1FBFFFFF

// Unknown
#define CART_DOM1_ADDR3_START     0x1FD00000
#define CART_DOM1_ADDR3_END       0x7FFFFFFF
