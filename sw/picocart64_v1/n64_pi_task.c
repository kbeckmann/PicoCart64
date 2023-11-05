/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include "n64_pi.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "hardware/irq.h"

#include "n64_defs.h"
#include "n64_pi.h"
#include "pc64_rand.h"
#include "pc64_regs.h"
#include "picocart64_pins.h"
#include "ringbuf.h"
#include "sram.h"
#include "stdio_async_uart.h"
#include "utils.h"
#include "picocart64_v1.h"

// The rom to load in normal .z64, big endian, format
#include "rom_vars.h"
#include "rom.h"

uint16_t rom_mapping[MAPPING_TABLE_LEN];

#if COMPRESSED_ROM
// do something
#else
static const uint16_t *rom_file_16 = (uint16_t *) rom_chunks;
#endif

RINGBUF_CREATE(ringbuf, 64, uint32_t);

// UART TX buffer
static uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

static inline uint32_t n64_pi_get_value(PIO pio)
{
	uint32_t value = pio_sm_get_blocking(pio, 0);

	// Disable to get some more performance. Enable for debugging.
	// Without ringbuf, ROM access takes 160-180ns. With, 240-260ns.
#if 0
	ringbuf_add(ringbuf, value);
#elif 0
	uart_print_hex_u32(value);
#endif

	return value;
}

void n64_pi_run(void)
{
	// Init PIO
	PIO pio = pio0;
	uint offset = pio_add_program(pio, &n64_pi_program);
	n64_pi_program_init(pio, 0, offset);
	pio_sm_set_enabled(pio, 0, true);

	// Wait for reset to be released
	while (gpio_get(N64_COLD_RESET) == 0) {
		tight_loop_contents();
	}

	uint32_t last_addr;
	uint32_t addr;
	uint32_t next_word;

	// Read addr manually before the loop
	addr = n64_pi_get_value(pio);

	while (1) {
		// addr must not be a WRITE or READ request here,
		// it should contain a 16-bit aligned address.
		// Assert drains performance, uncomment when debugging.
		// ASSERT((addr != 0) && ((addr & 1) == 0));

		// We got a start address
		last_addr = addr;

		// Handle access based on memory region
		// Note that the if-cases are ordered in priority from
		// most timing critical to least.
		if (last_addr >= CART_DOM1_ADDR2_START && last_addr <= CART_DOM1_ADDR2_END) {
			// Domain 1, Address 2 Cartridge ROM
			do {
				// Pre-fetch from the address
#if COMPRESSED_ROM
				uint32_t chunk_index = rom_mapping[(last_addr & 0xFFFFFF) >> COMPRESSION_SHIFT_AMOUNT];
				const uint16_t *chunk_16 = (const uint16_t *)rom_chunks[chunk_index];
				next_word = swap8(chunk_16[(last_addr & COMPRESSION_MASK) >> 1]);
#else
				next_word = swap8(rom_file_16[(last_addr & 0xFFFFFF) >> 1]);
#endif

				// Read command/address
				addr = n64_pi_get_value(pio);

				if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
				} else if ((addr & 0xffff0000) == 0xffff0000) {
					// WRITE
					// Ignore data since we're asked to write to the ROM.
					last_addr += 2;
				} else {
					// New address
					break;
				}
			} while (1);
		} else if (last_addr >= CART_SRAM_START && last_addr <= CART_SRAM_END) {
			// Domain 2, Address 2 Cartridge SRAM

			// Calculate start pointer
			uint16_t *sram_ptr = &sram[sram_resolve_address_shifted(last_addr)];

			do {
				// Read command/address
				addr = n64_pi_get_value(pio);

				if ((addr & 0xffff0000) == 0xffff0000) {
					// WRITE
					*(sram_ptr++) = addr & 0xFFFF;

					// More readable:
					// sram[sram_resolve_address_shifted(last_addr)] = addr & 0xFFFF;
					// last_addr += 2;
				} else if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, *(sram_ptr++));

					// More readable:
					// next_word = sram[sram_resolve_address_shifted(last_addr)];
					// pio_sm_put(pio, 0, next_word);
					// last_addr += 2;
				} else {
					// New address
					break;
				}
			} while (1);
		} else if (last_addr >= PC64_BASE_ADDRESS_START && last_addr <= PC64_BASE_ADDRESS_END) {
			// PicoCart64 BASE address space
			do {
				// Pre-fetch from the address
				next_word = pc64_uart_tx_buf[(last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1];

				// Read command/address
				addr = n64_pi_get_value(pio);

				if ((addr & 0xffff0000) == 0xffff0000) {
					// We got a WRITE
					// 0b11111111_11111111_xxxxxxxx_xxxxxxxx
					pc64_uart_tx_buf[(last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1] = swap8(addr);
					last_addr += 2;
				} else if (addr == 0) {
					// READ
					pio_sm_put(pio, 0, next_word);
					last_addr += 2;
					next_word = pc64_uart_tx_buf[(last_addr & (sizeof(pc64_uart_tx_buf) - 1)) >> 1];
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

				if ((addr & 0xffff0000) == 0xffff0000) {
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
						break;
					case PC64_REGISTER_FLASH_JEDEC_ID:
						next_word = g_flash_jedec_id;
						break;
					default:
						next_word = 0;
					}

					// Write as a 32-bit word
					pio_sm_put(pio, 0, next_word >> 16);
					last_addr += 2;

					// Get the next command/address
					addr = n64_pi_get_value(pio);
					if (addr != 0) {
						// Handle 16-bit reads even if we shouldn't get them here.
						continue;
					}

					pio_sm_put(pio, 0, next_word & 0xFFFF);
					last_addr += 2;
				} else if ((addr & 0xffff0000) == 0xffff0000) {
					// WRITE

					// Read two 16-bit half-words and merge them to a 32-bit value
					uint32_t write_word = addr << 16;
					write_word |= n64_pi_get_value(pio) & 0x0000FFFF;

					switch (last_addr - PC64_CIBASE_ADDRESS_START) {
					case PC64_REGISTER_UART_TX:
						stdio_uart_out_chars((const char *)pc64_uart_tx_buf, write_word & (sizeof(pc64_uart_tx_buf) - 1));
						break;
					case PC64_REGISTER_RAND_SEED:
						pc64_rand_seed(write_word);
						break;
					default:
						break;
					}

					last_addr += 4;
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
