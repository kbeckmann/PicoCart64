/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 * Copyright (c) 2022 Christopher Bonhage <me@christopherbonhage.com>
 *
 * Based on https://github.com/meeq/SaveTest-N64
 */

#include <string.h>
#include <stdint.h>
#include <libdragon.h>

#include "git_info.h"

// picocart64_shared
#include "pc64_regs.h"
#include "pc64_rand.h"
#include "n64_defs.h"

typedef struct PI_regs_s {
	volatile void *ram_address;
	uint32_t pi_address;
	uint32_t read_length;
	uint32_t write_length;
} PI_regs_t;
static volatile PI_regs_t *const PI_regs = (PI_regs_t *) 0xA4600000;

// SRAM constants
#define SRAM_256KBIT_SIZE         0x00008000
#define SRAM_768KBIT_SIZE         0x00018000
#define SRAM_1MBIT_SIZE           0x00020000
#define SRAM_BANK_SIZE            SRAM_256KBIT_SIZE
#define SRAM_256KBIT_BANKS        1
#define SRAM_768KBIT_BANKS        3
#define SRAM_1MBIT_BANKS          4

static void verify_memory_range(uint32_t base, uint32_t offset, uint32_t len)
{
	uint32_t start = base | offset;
	uint32_t end = start + len - 1;

	switch (base) {
	case CART_DOM2_ADDR1_START:
		assert(start >= CART_DOM2_ADDR1_START);
		assert(start <= CART_DOM2_ADDR1_END);
		assert(end >= CART_DOM2_ADDR1_START);
		assert(end <= CART_DOM2_ADDR1_END);
		break;

	case CART_DOM1_ADDR1_START:
		assert(start >= CART_DOM1_ADDR1_START);
		assert(start <= CART_DOM1_ADDR1_END);
		assert(end >= CART_DOM1_ADDR1_START);
		assert(end <= CART_DOM1_ADDR1_END);
		break;

	case CART_DOM2_ADDR2_START:
		assert(start >= CART_DOM2_ADDR2_START);
		assert(start <= CART_DOM2_ADDR2_END);
		assert(end >= CART_DOM2_ADDR2_START);
		assert(end <= CART_DOM2_ADDR2_END);
		break;

	case CART_DOM1_ADDR2_START:
		assert(start >= CART_DOM1_ADDR2_START);
		assert(start <= CART_DOM1_ADDR2_END);
		assert(end >= CART_DOM1_ADDR2_START);
		assert(end <= CART_DOM1_ADDR2_END);
		break;

	case CART_DOM1_ADDR3_START:
		assert(start >= CART_DOM1_ADDR3_START);
		assert(start <= CART_DOM1_ADDR3_END);
		assert(end >= CART_DOM1_ADDR3_START);
		assert(end <= CART_DOM1_ADDR3_END);
		break;

	case PC64_BASE_ADDRESS_START:
		assert(start >= PC64_BASE_ADDRESS_START);
		assert(start <= PC64_BASE_ADDRESS_END);
		assert(end >= PC64_BASE_ADDRESS_START);
		assert(end <= PC64_BASE_ADDRESS_END);
		break;

	case PC64_RAND_ADDRESS_START:
		assert(start >= PC64_RAND_ADDRESS_START);
		assert(start <= PC64_RAND_ADDRESS_END);
		assert(end >= PC64_RAND_ADDRESS_START);
		assert(end <= PC64_RAND_ADDRESS_END);
		break;

	case PC64_CIBASE_ADDRESS_START:
		assert(start >= PC64_CIBASE_ADDRESS_START);
		assert(start <= PC64_CIBASE_ADDRESS_END);
		assert(end >= PC64_CIBASE_ADDRESS_START);
		assert(end <= PC64_CIBASE_ADDRESS_END);
		break;

	default:
		assert(!"Unsupported base");
	}
}

static void pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len)
{
	assert(dest != NULL);
	verify_memory_range(base, offset, len);

	disable_interrupts();
	dma_wait();

	MEMORY_BARRIER();
	PI_regs->ram_address = UncachedAddr(dest);
	MEMORY_BARRIER();
	PI_regs->pi_address = offset | base;
	MEMORY_BARRIER();
	PI_regs->write_length = len - 1;
	MEMORY_BARRIER();

	enable_interrupts();
	dma_wait();
}

static void pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len)
{
	assert(src != NULL);
	verify_memory_range(base, offset, len);

	disable_interrupts();
	dma_wait();

	MEMORY_BARRIER();
	PI_regs->ram_address = UncachedAddr(src);
	MEMORY_BARRIER();
	PI_regs->pi_address = offset | base;
	MEMORY_BARRIER();
	PI_regs->read_length = len - 1;
	MEMORY_BARRIER();

	enable_interrupts();
	dma_wait();
}

static void pi_write_u32(const uint32_t value, uint32_t base, uint32_t offset)
{
	uint32_t buf[] = { value };

	data_cache_hit_writeback_invalidate(buf, sizeof(buf));
	pi_write_raw(buf, base, offset, sizeof(buf));
}

static uint8_t __attribute__((aligned(16))) facit_buf[SRAM_1MBIT_SIZE];
static uint8_t __attribute__((aligned(16))) read_buf[SRAM_1MBIT_SIZE];
static char __attribute__((aligned(16))) write_buf[0x1000];

static void pc64_uart_write(const uint8_t * buf, uint32_t len)
{
	// 16-bit aligned
	assert((((uint32_t) buf) & 0x1) == 0);

	uint32_t len_aligned32 = (len + 3) & (-4);

	data_cache_hit_writeback_invalidate((uint8_t *) buf, len_aligned32);
	pi_write_raw(write_buf, PC64_BASE_ADDRESS_START, 0, len_aligned32);

	pi_write_u32(len, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_UART_TX);
}

static void configure_sram(void)
{
	// PI registers
#define PI_BASE_REG          0x04600000
#define PI_STATUS_REG        (PI_BASE_REG+0x10)
#define	PI_STATUS_ERROR      0x04
#define PI_STATUS_IO_BUSY    0x02
#define PI_STATUS_DMA_BUSY   0x01

#define PI_BSD_DOM1_LAT_REG  (PI_BASE_REG+0x14)
#define PI_BSD_DOM1_PWD_REG  (PI_BASE_REG+0x18)
#define PI_BSD_DOM1_PGS_REG  (PI_BASE_REG+0x1C)
#define PI_BSD_DOM1_RLS_REG  (PI_BASE_REG+0x20)
#define PI_BSD_DOM2_LAT_REG  (PI_BASE_REG+0x24)
#define PI_BSD_DOM2_PWD_REG  (PI_BASE_REG+0x28)
#define PI_BSD_DOM2_PGS_REG  (PI_BASE_REG+0x2C)
#define PI_BSD_DOM2_RLS_REG  (PI_BASE_REG+0x30)

	// MIPS addresses
#define KSEG0 0x80000000
#define KSEG1 0xA0000000

	// Memory translation stuff
#define	PHYS_TO_K1(x)       ((uint32_t)(x)|KSEG1)
#define	IO_WRITE(addr,data) (*(volatile uint32_t *)PHYS_TO_K1(addr)=(uint32_t)(data))
#define	IO_READ(addr)       (*(volatile uint32_t *)PHYS_TO_K1(addr))

	// Initialize the PI
	IO_WRITE(PI_STATUS_REG, 3);

	// SDK default, 0x80371240
	//                  DCBBAA
	// AA = LAT
	// BB = PWD
	//  C = PGS
	//  D = RLS
	//
	// 1 cycle = 1/62.5 MHz = 16ns
	// Time = (register+1)*16ns
	// LAT = tL = 0x40 = 65 cycles
	// PWM = tP = 0x12 = 19 cycles
	// PGS      = 0x07 = 512 bytes page size
	// RLS = tR = 0x03 = 4 cycles

#if 0
	IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x40);
	IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x12);
	IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x07);
	IO_WRITE(PI_BSD_DOM1_RLS_REG, 0x03);

	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x12);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);

#else
	// Fast SRAM access (should match SDK values)
	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x0C);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);
#endif
}

int main(void)
{
	uint32_t *facit_buf32 = (uint32_t *) facit_buf;
	uint32_t *read_buf32 = (uint32_t *) read_buf;
	uint16_t *read_buf16 = (uint16_t *) read_buf;

	configure_sram();

	display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	console_init();
	// debug_init_isviewer();

	printf("PicoCart64 Test ROM (git rev %08X)\n\n", GIT_REV);

	///////////////////////////////////////////////////////////////////////////

	// Verify PicoCart64 Magic
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
	if (read_buf32[0] == PC64_MAGIC) {
		printf("[ OK ] MAGIC = 0x%08lX.\n", read_buf32[0]);
	} else {
		printf("[FAIL] MAGIC = 0x%08lX.\n", read_buf32[0]);
	}

	///////////////////////////////////////////////////////////////////////////

	// Print Hello from the N64
	strcpy(write_buf, "Hello from the N64!\r\n");
	pc64_uart_write((const uint8_t *)write_buf, strlen(write_buf));
	printf("[ -- ] Wrote hello over UART.\n");

	///////////////////////////////////////////////////////////////////////////

	// Initialize a random sequence
	pc64_rand_seed(0);
	data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
	for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
		facit_buf32[i] = pc64_rand32();
	}

	// Read back 1Mbit of SRAM
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

	// Compare SRAM with the facit
	if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
		printf("[FAIL] SRAM was not backed up properly.\n");

		for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
			if (facit_buf32[i] != read_buf[i]) {
				printf("       Error @%d: facit %08lX != sram %08lX\n", i, facit_buf32[i], read_buf32[i]);
				break;
			}
		}
	} else {
		printf("[ OK ] SRAM was backed up properly.\n");
	}

	///////////////////////////////////////////////////////////////////////////

	// Write 1Mbit of SRAM
	data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
	pi_write_raw(facit_buf, CART_DOM2_ADDR2_START, 0, sizeof(facit_buf));

	printf("[ -- ] Wrote test pattern to SRAM.\n");

	// Read it back and verify
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

	// Compare SRAM with the facit
	if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
		printf("[FAIL] SRAM (volatile) did not verify correctly.\n");

		for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
			if (facit_buf32[i] != read_buf[i]) {
				printf("       Error @%d: facit %08lX != sram %08lX\n", i, facit_buf32[i], read_buf32[i]);
				break;
			}
		}
	} else {
		printf("[ OK ] (volatile) SRAM verified correctly.\n");
	}

	///////////////////////////////////////////////////////////////////////////

	// Stress test: Read pseudo-random numbers from PicoCart64 RAND address space
	// Reset random seed
	pi_write_u32(0, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_RAND_SEED);
	pc64_rand_seed(0);

	// Compare buffer with RNG
	printf("[ -- ] RNG Test: ");
	bool rng_ok = true;
	for (int j = 0; j < 64 && rng_ok; j++) {
		// Read back 1Mbit of RAND values
		data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
		pi_read_raw(read_buf, PC64_RAND_ADDRESS_START, 0, sizeof(read_buf));
		printf(".");
		fflush(stdout);

		for (int i = 0; i < sizeof(read_buf) / sizeof(uint16_t); i++) {
			uint16_t value = pc64_rand16();

			if (value != read_buf16[i]) {
				printf("\n       Error @%d: ours %04X != theirs %04X", i, value, read_buf16[i]);
				rng_ok = false;
				break;
			}
		}
	}

	if (rng_ok) {
		printf("\n[ OK ] Random stress test verified correctly.\n");
	} else {
		printf("\n[FAIL] Random stress test failed.\n");
	}

	///////////////////////////////////////////////////////////////////////////

	// Read 1Mbit of 64DD IPL ROM
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, CART_DOM1_ADDR1_START, 0, sizeof(read_buf));
	printf("[ -- ] Read from the N64DD address space.\n");

	// Verify the PicoCart64 Magic *again*
	// This is done to ensure that the PicoCart64 is still alive and well,
	// and hasn't crashed or locked itself up.
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
	if (read_buf32[0] == PC64_MAGIC) {
		printf("[ OK ] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
	} else {
		printf("[FAIL] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
		printf("       PicoCart64 might stall now and require a power cycle.\n");
	}

	console_render();
}
