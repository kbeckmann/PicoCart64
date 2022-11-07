/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

// SRAM constants
#define SRAM_256KBIT_SIZE         0x00008000
#define SRAM_768KBIT_SIZE         0x00018000
#define SRAM_1MBIT_SIZE           0x00020000
#define SRAM_BANK_SIZE            SRAM_256KBIT_SIZE
#define SRAM_256KBIT_BANKS        1
#define SRAM_768KBIT_BANKS        3
#define SRAM_1MBIT_BANKS          4

extern uint8_t __attribute__((aligned(16))) facit_buf[SRAM_1MBIT_SIZE];
extern uint8_t __attribute__((aligned(16))) read_buf[SRAM_1MBIT_SIZE];
extern char __attribute__((aligned(16))) write_buf[0x1000];

void pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len);
void pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len);
void pi_write_u32(const uint32_t value, uint32_t base, uint32_t offset);
void pc64_uart_write(const uint8_t * buf, uint32_t len);
void verify_memory_range(uint32_t base, uint32_t offset, uint32_t len);
void configure_sram(void);