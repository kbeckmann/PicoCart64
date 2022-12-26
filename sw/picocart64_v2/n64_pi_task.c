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
#include "qspi_helper.h"

#include "rom.h"
#include "rom_vars.h"

#define USE_ROM_CACHE 0
#define DMA_READ_CMD 0x0B

#if USE_ROM_CACHE == 1
#define ROM_CACHE_SIZE 512 // in 32bit values
static volatile uint32_t rom_cache0[ROM_CACHE_SIZE];
static volatile uint32_t rom_cache1[ROM_CACHE_SIZE];
static volatile uint32_t *rom_cache; // pointer to current cache
static int rom_cache_index = 0;

static volatile uint32_t cache_startingAddress = 0;
static volatile uint32_t cache_endingAddress = 0;

static volatile uint32_t back_cache_startingAddress = 0;
static volatile uint32_t back_cache_endingAddress = 0;
#endif

// volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
// static const uint16_t *rom_file_16 = (uint16_t *) rom_chunks;

uint16_t rom_mapping[MAPPING_TABLE_LEN];

#if COMPRESSED_ROM
// do something
#else
static const uint16_t *rom_file_16 = (uint16_t *) rom_chunks;
#endif


static inline void psram_set_cs2(uint8_t chip)
{
	uint32_t mask = (1 << PIN_DEMUX_IE) | (1 << PIN_DEMUX_A0) | (1 << PIN_DEMUX_A1) | (1 << PIN_DEMUX_A2);
	uint32_t new_mask;

	if (chip >= 1 && chip <= 8) {
		chip--;					// convert to 0-indexed
		new_mask = (1 << PIN_DEMUX_IE) | (chip << PIN_DEMUX_A0);
	} else {
		// Set PIN_DEMUX_IE = 0 to pull all PSRAM CS-lines high
		new_mask = 0;
	}

	uint32_t old_gpio_out = sio_hw->gpio_out;
	sio_hw->gpio_out = (old_gpio_out & (~mask)) | new_mask;
}

volatile uint32_t update_rom_cache_for_address = 0;
void update_rom_cache(uint32_t address) {
	#if USE_ROM_CACHE == 1
	back_cache_startingAddress = address;
	back_cache_endingAddress = address + ROM_CACHE_SIZE;

	// Load the values into other rom cache, so if current index is 0, load in 1, and if 1 load into 0
	if (rom_cache_index == 0) {
		psram_set_cs2(DEBUG_CS_CHIP_USE);
		flash_bulk_read(DMA_READ_CMD, (uint32_t*)rom_cache1, address, ROM_CACHE_SIZE, 0);
		psram_set_cs2(0);
	} else {
		psram_set_cs2(DEBUG_CS_CHIP_USE);
		flash_bulk_read(DMA_READ_CMD, (uint32_t*)rom_cache0, 0, ROM_CACHE_SIZE, 0);
		psram_set_cs2(0);
	}
	#endif
}

static inline void swap_rom_cache() {
	#if USE_ROM_CACHE == 1
	cache_startingAddress = back_cache_startingAddress;
	cache_endingAddress = back_cache_endingAddress;
	if (rom_cache_index == 0) {
		rom_cache = rom_cache1;
		rom_cache_index = 1;
	} else if (rom_cache_index == 1) {
		rom_cache = rom_cache0;
		rom_cache_index = 0;
	}
	#endif
}

// static inline uint16_t read_from_psram(uint32_t rom_address) {
// 	// convert the rom_address into an address we can use for getting data out of the cache
// 	// rom_address should be 2 byte aligned, which means we are likely to get address 0,2,4
// 	// and what we really want are 4 byte aligned addresses
	
// 	uint32_t index = (rom_address & 0xFFFFFF) >> 1; // address / 2 (works when storing 16bit values per index)
// 	// uint32_t index = (rom_address & 0xFFFFFF) >> 2; // address / 4 (works when storing 32bit values per index)
// 	#if USE_ROM_CACHE == 1
// 	/* If we are already have way through the current cache, start updating for the next set of values */
// 	if (index >= cache_startingAddress && index <= cache_endingAddress && index >= cache_endingAddress >> 2) {
// 		if (update_rom_cache_for_address != cache_endingAddress) {
// 			update_rom_cache_for_address = cache_endingAddress;
// 			multicore_fifo_push_blocking(CORE1_UPDATE_ROM_CACHE);
// 		}

// 		// Calculate the index into the cache
// 		uint32_t cache_index = index - cache_startingAddress;
// 		return swap16(rom_cache[cache_index]);

// 	/* Use the cached value*/
// 	} else if (index >= cache_startingAddress && index <= cache_endingAddress) {
// 		// Calculate the index into the cache
// 		uint32_t cache_index = index - cache_startingAddress;
// 		return swap16(rom_cache[cache_index]);

// 	/* If we have cached these values, use those*/
// 	} else if 
// 	(
// 		index >= cache_endingAddress || index <= cache_startingAddress &&
// 		index >= back_cache_startingAddress && index <= back_cache_endingAddress
// 	) {
// 		swap_rom_cache();

// 		uint32_t cache_index = index - cache_startingAddress;
// 		return swap16(rom_cache[cache_index]);

// 	} else {
// 		// A total cache miss, update
// 		add_log_to_buffer(rom_address);
// 		update_rom_cache_for_address = index;
// 		multicore_fifo_push_blocking(CORE1_UPDATE_ROM_CACHE);
// 	}
// 	#endif
	
// 	uint16_t word = ptr16[index];
// 	return word;
// }

void load_rom_cache(uint32_t startingAt) {
	#if USE_ROM_CACHE == 1
	cache_startingAddress = startingAt;
	cache_endingAddress = startingAt + ROM_CACHE_SIZE;
	rom_cache = rom_cache0;
	rom_cache_index = 0;

	uint32_t totalTime = 0;
	uint32_t now = time_us_32();
	psram_set_cs2(DEBUG_CS_CHIP_USE);
	flash_bulk_read(DMA_READ_CMD, (uint32_t*)rom_cache, startingAt, ROM_CACHE_SIZE, 0);
	psram_set_cs2(0);
	totalTime += time_us_32() - now;
	for(int i = 0; i < 16; i++) {
        printf("ROM_CACHE[%d]: %08x\n",i, rom_cache[i]);
    }

	printf("Loaded %d (32-bit values) of data in %d us.\n", ROM_CACHE_SIZE, totalTime);
	#else
	printf("No rom cache in use. Set USE_ROM_CACHE to 1 to use.\n");
	#endif
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

	// Init PIO
	PIO pio = pio0;
	uint offset = pio_add_program(pio, &n64_pi_program);
	n64_pi_program_init(pio, 0, offset);
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
	while (1) {
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
			// Configure bus to run slowly.
			// This is better patched in the rom, so we won't need a branch here.
			// But let's keep it here so it's easy to import roms.

			// 0x8037FF40 in big-endian
			next_word = 0x8037;
			addr = n64_pi_get_value(pio);
			// uart_print_hex_u32(addr);

			// Assume addr == 0, i.e. READ request
			pio_sm_put(pio, 0, next_word);
			last_addr += 2;

			// Patch bus speed here if needed (e.g. if not overclocking)
			next_word = 0xFF40;
			// next_word = 0x2040;

			// Official SDK standard speed
			// next_word = 0x1240;
			addr = n64_pi_get_value(pio);
			// uart_print_hex_u32(addr);

			// Assume addr == 0, i.e. push 16 bits of data
			pio_sm_put(pio, 0, next_word);
			last_addr += 2;

			// Pre-fetch
#if COMPRESSED_ROM
			uint32_t chunk_index = rom_mapping[(last_addr & 0xFFFFFF) >> COMPRESSION_SHIFT_AMOUNT];
			const uint16_t *chunk_16 = (const uint16_t *)rom_chunks[chunk_index];
			next_word = chunk_16[(last_addr & COMPRESSION_MASK) >> 1];
#else
			next_word = rom_file_16[(last_addr & 0xFFFFFF) >> 1];
			// next_word = ptr16[(last_addr & 0xFFFFFF) >> 1];
			// add_log_to_buffer((last_addr & 0xFFFFFF));
#endif

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
			} while (1);
		} else if (last_addr >= 0x10000000 && last_addr <= 0x1FBFFFFF) {
			// Domain 1, Address 2 Cartridge ROM
			do {
				// Pre-fetch from the address
#if COMPRESSED_ROM
				uint32_t chunk_index = rom_mapping[(last_addr & 0xFFFFFF) >> COMPRESSION_SHIFT_AMOUNT];
				const uint16_t *chunk_16 = (const uint16_t *)rom_chunks[chunk_index];
				next_word = chunk_16[(last_addr & COMPRESSION_MASK) >> 1];
				
				// Using the compressed from from the header
				// next_word = read_from_psram(last_addr);
#else
				next_word = rom_file_16[(last_addr & 0xFFFFFF) >> 1];
				// next_word = ptr16[(last_addr & 0xFFFFFF) >> 1];
				// add_log_to_buffer((last_addr & 0xFFFFFF));
#endif
				addr = n64_pi_get_value(pio);
			add_log_to_buffer(0xbbbbAAAA);
			add_log_to_buffer(addr);
			add_log_to_buffer(0xbbbbBBBB);
			add_log_to_buffer(last_addr);

				if (addr == 0) {
					// READ
 handle_d1a2_read:
			add_log_to_buffer(0xaaaaaaaa);
					pio_sm_put(pio, 0, swap8(next_word));
					last_addr += 2;
				} else if (addr & 0x00000001) {
			add_log_to_buffer(0xbbbbCCCC);
			add_log_to_buffer(addr);
			add_log_to_buffer(0xbbbbDDDD);
					// WRITE
					// Ignore data since we're asked to write to the ROM.
					last_addr += 2;
				} else {
					// New address
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
				next_word = pc64_uart_tx_buf[buf_index];

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr & 0x00000001) {
					// We got a WRITE
					// 0bxxxxxxxx_xxxxxxxx_11111111_11111111
					//pc64_uart_tx_buf[(last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1] = swap8(addr >> 16);
					last_addr += 2;
				} else if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;

				} else {
					// New address
					break;
				}
			} while (1);
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
						next_word = sd_is_busy ? 0x00000001 : 0x00000000;
						pio_sm_put(pio, 0, next_word);
						
						break;
					default:
						next_word = 0;
					}

					last_addr += 2;
					
				} else if (addr & 0x00000001) {
					// WRITE

					// Read two 16-bit half-words and merge them to a 32-bit value
					uint32_t write_word = addr & 0xFFFF0000;
					uint16_t half_word = addr >> 16;
					uint addr_advance = 4;

					switch (last_addr - PC64_CIBASE_ADDRESS_START) {
					case PC64_REGISTER_UART_TX:
						write_word |= n64_pi_get_value(pio) >> 16;
						//stdio_uart_out_chars((const char *)pc64_uart_tx_buf, write_word & (sizeof(pc64_uart_tx_buf) - 1));
						break;

					case PC64_REGISTER_RAND_SEED:
						write_word |= n64_pi_get_value(pio) >> 16;
						pc64_rand_seed(write_word);
						break;

					case PC64_COMMAND_SD_READ:
						write_word |= n64_pi_get_value(pio) >> 16;
						multicore_fifo_push_blocking(CORE1_SEND_SD_READ_CMD);
						break;

					case PC64_REGISTER_SD_READ_SECTOR0:
						addr_advance = 2;
						pc64_set_sd_read_sector_part(0, half_word);
						break;
					case PC64_REGISTER_SD_READ_SECTOR1:
						addr_advance = 2;
						pc64_set_sd_read_sector_part(1, half_word);
						break;
					case PC64_REGISTER_SD_READ_SECTOR2:
						addr_advance = 2;
						pc64_set_sd_read_sector_part(2, half_word);
						break;
					case PC64_REGISTER_SD_READ_SECTOR3:
						addr_advance = 2;
						pc64_set_sd_read_sector_part(3, half_word);
						break;

					case PC64_REGISTER_SD_READ_NUM_SECTORS:
						write_word |= n64_pi_get_value(pio) >> 16;
						pc64_set_sd_read_sector_count(write_word);
						break;

					case PC64_REGISTER_SD_SELECT_ROM:
						// Rom is requesting we load rom
						// it will send the title of the rom
						// and we should load it
						// TODO: This should probably be turned into a CMD+Register thing
						// where n64 writes to a picocart register and 
						// then sends the command, so we can do things like open directories
						// and traverse the filesystem

						break;

					default:
						break;
					}

					last_addr += addr_advance;
				} else {
					// New address
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
			// Read to empty fifo
			addr = n64_pi_get_value(pio);

			// Jump to start of the PIO program.
			pio_sm_exec(pio, 0, pio_encode_jmp(offset + 0));

			// Read and handle the following requests normally
			addr = n64_pi_get_value(pio);
		}
	}
}
