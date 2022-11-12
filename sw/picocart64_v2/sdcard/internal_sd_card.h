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
#include "pio_uart/pio_uart.h"

#define SD_CARD_SECTOR_SIZE 512 // 512 bytes

extern int PC64_MCU_ID;
extern volatile bool sendDataReady;

// UART TX buffer
extern volatile uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

// set the sector to start reading from
void pc64_set_sd_read_sector(uint64_t sector);

void pc64_set_sd_read_sector_part(int index, uint16_t value);

// set the number of sectors to read
void pc64_set_sd_read_sector_count(uint32_t count);

// Set selected rom title, max 256 characters
void pc64_set_sd_rom_selection(char* title);

void pc64_send_sd_read_command(void);

extern volatile bool sd_is_busy;
//bool is_sd_busy();

// UART RX methods, unique per MCU
void mcu1_process_rx_buffer();
void mcu2_process_rx_buffer();

// Variables are extracted via mcu2_process_rx_buffer
// Then data is read from the SD Card and sent over uart
void send_sd_card_data();

// SD Card functions
void mount_sd(void);

// sd/rom/psram stuff
void load_rom(const char *filename);
void test_read_from_psram();
