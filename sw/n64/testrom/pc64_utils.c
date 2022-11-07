/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 * 
 */

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <libdragon.h>
#include "pc64_regs.h"
#include "pc64_rand.h"
#include "n64_defs.h"
#include "pc64_utils.h"

typedef struct PI_regs_s {
	volatile void *ram_address;
	uint32_t pi_address;
	uint32_t read_length;
	uint32_t write_length;
} PI_regs_t;
static volatile PI_regs_t *const PI_regs = (PI_regs_t *) 0xA4600000;

uint8_t __attribute__((aligned(16))) facit_buf[SRAM_1MBIT_SIZE];
uint8_t __attribute__((aligned(16))) read_buf[SRAM_1MBIT_SIZE];
char __attribute__((aligned(16))) write_buf[0x1000];

void verify_memory_range(uint32_t base, uint32_t offset, uint32_t len)
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

void configure_sram(void)
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

void pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len)
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

void pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len)
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

void pi_write_u32(const uint32_t value, uint32_t base, uint32_t offset)
{
	uint32_t buf[] = { value };

	data_cache_hit_writeback_invalidate(buf, sizeof(buf));
	pi_write_raw(buf, base, offset, sizeof(buf));
}

void pc64_uart_write(const uint8_t * buf, uint32_t len)
{
	// 16-bit aligned
	assert((((uint32_t) buf) & 0x1) == 0);

	uint32_t len_aligned32 = (len + 3) & (-4);

	data_cache_hit_writeback_invalidate((uint8_t *) buf, len_aligned32);
	pi_write_raw(write_buf, PC64_BASE_ADDRESS_START, 0, len_aligned32);

	pi_write_u32(len, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_UART_TX);
}