/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill <kaili.hill25@gmail.com>
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pc64_regs.h"

#define SD_CARD_SECTOR_SIZE 512 // 512 bytes

/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Successful */
	RES_ERROR,		/* 1: R/W Error */
	RES_WRPRT,		/* 2: Write Protected */
	RES_NOTRDY,		/* 3: Not Ready */
	RES_PARERR		/* 4: Invalid Parameter */
} DRESULT;

// UART TX buffer
extern uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

// set the sector to start reading from
void pc64_set_sd_read_sector(uint64_t sector);

void pc64_set_sd_read_sector_part(uint index, uint16_t value);

// set the number of sectors to read
void pc64_set_sd_read_sector_count(uint32_t count);

// Set selected rom title, max 256 characters
void pc64_set_sd_rom_selection(char* title);

void pc64_send_sd_read_command(void);

bool is_sd_busy();

void on_uart_rx(void);
