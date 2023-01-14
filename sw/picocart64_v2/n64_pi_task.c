/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include "n64_pi.h"

#include <stdio.h>
#include <string.h>

// #include "pico/stdlib.h"
// #include "pico/stdio.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/structs/systick.h"

#include "n64_defs.h"
#include "n64_pi_task.h"
#include "pc64_rand.h"
#include "pc64_regs.h"
#include "pins_mcu1.h"
#include "ringbuf.h"
#include "sram.h"
#include "stdio_async_uart.h"
#include "utils.h"

#include "sdcard/internal_sd_card.h"
#include "psram.h"
#include "program_flash_array.h"

#include "rom.h"
// const char __attribute__((section(".n64_rom.header"))) picocart_header[16] = "picocartcompress";
// const uint16_t __attribute__((section(".n64_rom.mapping"))) flash_rom_mapping[] = {
// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 202, 201, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 356, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 356, 356, 356, 356, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420, 421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 448, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 201, 201
// };
// const unsigned char __attribute__((section(".n64_rom"))) rom_chunks[][1024];
// #define COMPRESSED_ROM 1
// #define COMPRESSION_SHIFT_AMOUNT 10
// #define COMPRESSION_MASK 1023

#include "rom_vars.h"

#define DMA_READ_CMD 0x0B

volatile int g_currentMemoryArrayChip = 3;
 // Used when addressing chips outside the starting one
volatile uint32_t address_modifier = 0;
volatile bool g_loadRomFromMemoryArray = false;
static uint n64_pi_pio_offset;
// volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;
volatile uint16_t *ptr16 = (volatile uint16_t *)0x13000000; // no cache

uint16_t rom_mapping[MAPPING_TABLE_LEN];

#if COMPRESSED_ROM
// do something
#else
static const uint16_t *rom_file_16 = (uint16_t *) rom_chunks;
#endif

#define PSRAM_ADDRESS_MODIFIER_0 (0)
#define PSRAM_ADDRESS_MODIFIER_1 (PSRAM_CHIP_CAPACITY_BYTES)
#define PSRAM_ADDRESS_MODIFIER_2 (PSRAM_CHIP_CAPACITY_BYTES * 2) 
#define PSRAM_ADDRESS_MODIFIER_3 (PSRAM_CHIP_CAPACITY_BYTES * 3) 
#define PSRAM_ADDRESS_MODIFIER_4 (PSRAM_CHIP_CAPACITY_BYTES * 4) 
#define PSRAM_ADDRESS_MODIFIER_5 (PSRAM_CHIP_CAPACITY_BYTES * 5) 
#define PSRAM_ADDRESS_MODIFIER_6 (PSRAM_CHIP_CAPACITY_BYTES * 6)
uint32_t g_addressModifierTable[] = {
	0, // chip 1 
	0, 
	PSRAM_ADDRESS_MODIFIER_0, // currently psram starts at chip 3
	PSRAM_ADDRESS_MODIFIER_1,
	PSRAM_ADDRESS_MODIFIER_2,
	PSRAM_ADDRESS_MODIFIER_3,
	PSRAM_ADDRESS_MODIFIER_4,
	PSRAM_ADDRESS_MODIFIER_5
};
volatile uint32_t tempChip = 0;
inline uint16_t rom_read(uint32_t rom_address) {
#if COMPRESSED_ROM
	if (!g_loadRomFromMemoryArray) {
		uint32_t chunk_index = rom_mapping[(rom_address & 0xFFFFFF) >> COMPRESSION_SHIFT_AMOUNT];
		const uint16_t *chunk_16 = (const uint16_t *)rom_chunks[chunk_index];
		return chunk_16[(rom_address & COMPRESSION_MASK) >> 1];
	} else {
		tempChip = psram_addr_to_chip(rom_address);
		if (tempChip != g_currentMemoryArrayChip) {
			g_currentMemoryArrayChip = tempChip;

			// Set the new chip
			psram_set_cs(g_currentMemoryArrayChip);
		}
		return ptr16[(((rom_address - g_addressModifierTable[g_currentMemoryArrayChip]) & 0xFFFFFF) >> 1)];
	}
#else
	return rom_file_16[(last_addr & 0xFFFFFF) >> 1];
#endif
}

void restart_n64_pi_pio() {
	PIO pio = pio0;
	// Stop pio so it's safe to reenter n64_pi_run method
	pio_sm_set_enabled(pio, 0, false);
	pio_remove_program(pio, &n64_pi_program, n64_pi_pio_offset);

	// Start it again
	// n64_pi_pio_offset = pio_add_program(pio, &n64_pi_program);
	// n64_pi_program_init(pio, 0, n64_pi_pio_offset);
	// pio_sm_set_enabled(pio, 0, true);
}

static inline uint32_t resolve_sram_address(uint32_t address)
{	
	uint32_t bank = (address >> 18) & 0x3;
	uint32_t resolved_address;

	if (bank) {
		resolved_address = address & (SRAM_256KBIT_SIZE - 1);
		resolved_address |= bank << 15;
	} else {
		resolved_address = address & (sizeof(sram) - 1);
	}

	return resolved_address;
}

static inline uint32_t n64_pi_get_value(PIO pio)
{
	uint32_t value = pio_sm_get_blocking(pio, 0);
	return value;
}

void __no_inline_not_in_flash_func(n64_pi_run)(void)
{
	systick_hw->csr = 0x5;
    systick_hw->rvr = 0x00FFFFFF;

	// Probably already restarted or first time start, we want to run the loop
	// until this is true, so always reset it
	g_restart_pi_handler = false;

	g_currentMemoryArrayChip = START_ROM_LOAD_CHIP_INDEX;

	// Init PIO
	PIO pio = pio0;
	n64_pi_pio_offset = pio_add_program(pio, &n64_pi_program);
	n64_pi_program_init(pio, 0, n64_pi_pio_offset);
	pio_sm_set_enabled(pio, 0, true);

	// Wait for reset to be released
	while (gpio_get(PIN_N64_COLD_RESET) == 0) {
		tight_loop_contents(); 
	}

	// volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;
	volatile uint32_t last_addr;
	volatile uint32_t addr;
	volatile uint32_t next_word;

	// Read addr manually before the loop
	addr = n64_pi_get_value(pio);

	// add_log_to_buffer(addr);
	uint32_t lastUpdate = 0;
	while (1 && !g_restart_pi_handler) {
		// addr must not be a WRITE or READ request here,
		// it should contain a 16-bit aligned address.
		// Assert drains performance, uncomment when debugging.
		// ASSERT((addr != 0) && ((addr & 1) == 0));

		// We got a start address
		last_addr = addr;
		// add_log_to_buffer(last_addr);
		// add_log_to_buffer(last_addr);

		// Handle access based on memory region
		// Note that the if-cases are ordered in priority from
		// most timing critical to least.
		if (last_addr == 0x10000000) {
			
			// Chirp, reading 0
			// uart_tx_program_putc(0x10);

			// Configure bus to run slowly.
			// This is better patched in the rom, so we won't need a branch here.
			// But let's keep it here so it's easy to import roms.

			// uart_tx_program_putc(0xA);

			// 0x8037FF40 in big-endian
			next_word = 0x8037;
			addr = n64_pi_get_value(pio);
			// uart_print_hex_u32(addr);

			// Assume addr == 0, i.e. READ request
			pio_sm_put(pio, 0, next_word);
			last_addr += 2;

			// Patch bus speed here if needed (e.g. if not overclocking)
			// next_word = 0x2040;
			// if (g_loadRomFromMemoryArray) {
			// 	// Official SDK standard speed
				// next_word = 0x1240;
			// } else {
				next_word = 0xFF40;
			// }

			// Slowest speed
			// next_word = 0xFF40;

			// next_word = 0x2040;
		
			addr = n64_pi_get_value(pio);

			// Assume addr == 0, i.e. push 16 bits of data
			pio_sm_put(pio, 0, next_word);
			last_addr += 2;

			// Pre-fetch
			next_word = rom_read(last_addr);

			// ROM patching done
			addr = n64_pi_get_value(pio);
			// uart_print_hex_u32(addr);
			if (addr == 0) {
				// I apologise for the use of goto, but it seemed like a fast way
				// to enter the next state immediately.
				goto handle_d1a2_read;
			} else {
				continue;
			}
		} else if (last_addr >= CART_SRAM_START && last_addr <= CART_SRAM_END) {
			// Domain 2, Address 2 Cartridge SRAM
			do {
				// Pre-fetch from the address
				next_word = sram[resolve_sram_address(last_addr) >> 1];

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr & 0x00000001) {
					// We got a WRITE
					// 0bxxxxxxxx_xxxxxxxx_11111111_11111111
					sram[resolve_sram_address(last_addr) >> 1] = addr >> 16;
					last_addr += 2;
				} else if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
					next_word = sram[resolve_sram_address(last_addr) >> 1];
				} else {
					// New address
					break;
				}

				if (g_restart_pi_handler) {
					break;
				}
			} while (1);
		} else if (last_addr >= 0x10000000 && last_addr <= 0x1FBFFFFF) {
			// Domain 1, Address 2 Cartridge ROM
			do {
				// Pre-fetch from the address
				next_word = rom_read(last_addr);
				// uart_tx_program_putc((uint8_t)(next_word >> 8));
				// uart_tx_program_putc((uint8_t)(next_word));

				// uint32_t chunk_index = rom_mapping[(last_addr & 0xFFFFFF) >> COMPRESSION_SHIFT_AMOUNT];
				// const uint16_t *chunk_16 = (const uint16_t *)rom_chunks[chunk_index];
				// next_word = chunk_16[(last_addr & COMPRESSION_MASK) >> 1];

				addr = n64_pi_get_value(pio);
				// printf("%04x\n", addr);

				if (addr == 0) {
					// READ
 handle_d1a2_read:
					// uart_tx_program_putc(0xD);
					pio_sm_put(pio, 0, swap8(next_word));
					last_addr += 2;
				} else if (addr & 0x00000001) {
					// WRITE
					// Ignore data since we're asked to write to the ROM.
					last_addr += 2;
				} else {
					// New address
					break;
				}

				if (g_restart_pi_handler) {
					break;
				}
			} while (1);
		}
#if 0
		else if (last_addr >= 0x05000000 && last_addr <= 0x05FFFFFF) {
			// Domain 2, Address 1 N64DD control registers
			do {
				// We don't support this yet, but we have to consume another value
				next_word = 0;

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
				} else if (addr & 0x00000001) {
					// WRITE
					// Ignore
					last_addr += 2;
				} else {
					// New address
					break;
				}
			} while (1);
		} else if (last_addr >= 0x06000000 && last_addr <= 0x07FFFFFF) {
			// Domain 1, Address 1 N64DD IPL ROM (if present)
			do {
				// We don't support this yet, but we have to consume another value
				next_word = 0;

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
				} else if (addr & 0x00000001) {
					// WRITE
					// Ignore
					last_addr += 2;
				} else {
					// New address
					break;
				}
			} while (1);
		}
#endif
		else if (last_addr >= PC64_BASE_ADDRESS_START && last_addr <= PC64_BASE_ADDRESS_END) {
			// PicoCart64 BASE address space
			do {
				// Pre-fetch from the address
				uint32_t buf_index = (last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1;
				//next_word = PC64_MAGIC;//swap8(pc64_uart_tx_buf[buf_index]);

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr & 0x00000001) {
					// We got a WRITE
					// 0bxxxxxxxx_xxxxxxxx_11111111_11111111
					// pc64_uart_tx_buf[(last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1] = swap8(addr >> 16);
					pc64_uart_tx_buf[buf_index] = swap8(addr >> 16);
					last_addr += 2;
				} else if (addr == 0) {
					// READ
					next_word = pc64_uart_tx_buf[buf_index];
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;

				} else {
					// New address
					break;
				}

				if (g_restart_pi_handler) {
					break;
				}
			} while (1);
		
		} else if (last_addr >= PC64_CIBASE_ADDRESS_START && last_addr <= PC64_CIBASE_ADDRESS_END) {
			// PicoCart64 CIBASE address space
			do {
				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr == 0) {
					// READ
					switch (last_addr - PC64_CIBASE_ADDRESS_START) {
					case PC64_REGISTER_MAGIC:
						next_word = PC64_MAGIC;

						// Write as a 32-bit word
						pio_sm_put(pio, 0, next_word >> 16);
						last_addr += 2;
						// Get the next command/address
						addr = n64_pi_get_value(pio);
						if (addr != 0) {
							continue;
						}

						pio_sm_put(pio, 0, next_word & 0xFFFF);

						break;
					case PC64_REGISTER_SD_BUSY:
						// next_word = sd_is_busy ? 0x00000001 : 0x00000000;
						
						// Upper 16 bits are just 0
						pio_sm_put(pio, 0, 0x0000);

						// last_addr += 2;

						// // Get the next command/address
						// addr = n64_pi_get_value(pio);
						// if (addr != 0) {
						// 	continue;
						// }

						// // now we can send the actual busy bit
						// if (sd_is_busy) {
						// 	pio_sm_put(pio, 0, 0x0001);
						// } else {
						// 	pio_sm_put(pio, 0, 0x0000);
						// }
						
						break;
					case (PC64_REGISTER_SD_BUSY + 2):
						if (sd_is_busy) {
							pio_sm_put(pio, 0, 0x0001);
						} else {
							pio_sm_put(pio, 0, 0x0000);
						}
						break;

					default:
						next_word = 0;
					}

					last_addr += 2;
					
				} else if (addr & 0x00000001) {
					// WRITE

					// Read two 16-bit half-words and merge them to a 32-bit value
					uint32_t write_word = addr & 0xFFFF0000;
					// uint16_t half_word = addr >> 16;
					uint addr_advance = 2;

					switch (last_addr - PC64_CIBASE_ADDRESS_START) {
					case PC64_REGISTER_UART_TX:
						write_word |= n64_pi_get_value(pio) >> 16;
						//stdio_uart_out_chars((const char *)pc64_uart_tx_buf, write_word & (sizeof(pc64_uart_tx_buf) - 1));
						addr_advance = 4;
						break;

					case PC64_REGISTER_RAND_SEED:
						write_word |= n64_pi_get_value(pio) >> 16;
						pc64_rand_seed(write_word);
						addr_advance = 4;
						break;

					case PC64_COMMAND_SD_READ:
						// write_word |= n64_pi_get_value(pio) >> 16;
						// multicore_fifo_push_blocking(CORE1_SEND_SD_READ_CMD);
						break;

					case (PC64_COMMAND_SD_READ + 2):
						multicore_fifo_push_blocking(CORE1_SEND_SD_READ_CMD);
						break;

					case PC64_REGISTER_SD_READ_SECTOR0:
						// write_word |= n64_pi_get_value(pio) >> 16;
						// pc64_set_sd_read_sector_part(0, write_word);
						pc64_set_sd_read_sector_part(0, write_word);
						break;

					case (PC64_REGISTER_SD_READ_SECTOR0+2):
						pc64_set_sd_read_sector_part(1, write_word);
						break;

					case PC64_REGISTER_SD_READ_SECTOR1:
						// write_word |= n64_pi_get_value(pio) >> 16;
						// pc64_set_sd_read_sector_part(1, write_word);
						pc64_set_sd_read_sector_part(2, write_word);
						break;

					case (PC64_REGISTER_SD_READ_SECTOR1 + 2):
						pc64_set_sd_read_sector_part(3, write_word);
						break;

					case PC64_REGISTER_SD_READ_NUM_SECTORS:
						// write_word |= n64_pi_get_value(pio) >> 16;
						// pc64_set_sd_read_sector_count(1, write_word);
						pc64_set_sd_read_sector_count(1, write_word);
						break;

					case (PC64_REGISTER_SD_READ_NUM_SECTORS + 2):
						pc64_set_sd_read_sector_count(0, write_word);
						break;

					case PC64_REGISTER_SD_SELECT_ROM:
						// write_word |= n64_pi_get_value(pio) >> 16;
						break;
					case (PC64_REGISTER_SD_SELECT_ROM + 2):
						pc64_set_sd_rom_selection((char *)pc64_uart_tx_buf, write_word);
						multicore_fifo_push_blocking(CORE1_LOAD_NEW_ROM_CMD);
						break;

					default:
						break;
					}

					last_addr += addr_advance;
				} else {
					// New address
					break;
				}

				if (g_restart_pi_handler) {
					break;
				}
			} while (1);
		} else if (last_addr >= 0x81000000 && last_addr <= 0x81001000) {
			uart_tx_program_putc(0x09);
			uart_tx_program_putc(0x08);
			uart_tx_program_putc(0x07);
			// Read to empty fifo
			addr = n64_pi_get_value(pio);

			// Jump to start of the PIO program.
			pio_sm_exec(pio, 0, pio_encode_jmp(n64_pi_pio_offset + 0));

			// Read and handle the following requests normally
			addr = n64_pi_get_value(pio);
		} else if (last_addr >= PC64_RAND_ADDRESS_START && last_addr <= PC64_RAND_ADDRESS_END) {
			// PicoCart64 RAND address space
			do {
				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr & 0x00000001) {
					// We got a WRITE
					last_addr += 2;
				} else if (addr == 0) {
					// READ
					next_word = pc64_rand16();
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
				} else {
					// New address
					break;
				}

				if (g_restart_pi_handler) {
					break;
				}
			} while (1);
		} else {
			// Don't handle this request - jump back to the beginning.
			// This way, there won't be a bus conflict in case e.g. a physical N64DD is connected.

			// Enable to log addresses to UART
#if 0
			uart_print_hex_u32(last_addr);
#endif

			if (g_restart_pi_handler) {
				break;
			}

			// Read to empty fifo
			addr = n64_pi_get_value(pio);

			// Jump to start of the PIO program.
			pio_sm_exec(pio, 0, pio_encode_jmp(n64_pi_pio_offset + 0));

			// Read and handle the following requests normally
			addr = n64_pi_get_value(pio);
		}

		if (g_restart_pi_handler) {
			break;
		}
	}

	// Tear down the pio sm so the function can be called again.
	pio_sm_set_enabled(pio, 0, false);
	pio_remove_program(pio, &n64_pi_program, n64_pi_pio_offset);
}