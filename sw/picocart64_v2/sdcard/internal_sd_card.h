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

#define ERASE_AND_WRITE_TO_FLASH_ARRAY 0
#define LOAD_TO_PSRAM_ARRAY 2 // 1 if use psram, 0 to use flash, 2 = do nothing?
#define SD_CARD_SECTOR_SIZE 512 // 512 bytes

extern int PC64_MCU_ID;
extern volatile bool sendDataReady;
extern volatile bool startRomLoad;
extern volatile bool romLoading;

// debug vars
extern int totalSectorsRead;
extern int numberOfSendDataCalls;
extern uint32_t totalTimeOfSendData_ms;

// UART TX buffer
extern volatile uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

// set the sector to start reading from
void pc64_set_sd_read_sector(uint64_t sector);

void pc64_set_sd_read_sector_part(int index, uint32_t value);

// set the number of sectors to read
void pc64_set_sd_read_sector_count(int index, uint32_t count);

// Set selected rom title, max 256 characters
void pc64_set_sd_rom_selection(char* titleBuffer, uint32_t len);

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

/* sd/rom/psram stuff */
// loads the rom file specified in sd_selected_rom_title, that is set with the load rom command from mcu1
void load_selected_rom(); 
void load_rom(const char *filename);

void pc64_send_load_new_rom_command();
void load_new_rom(char* filename);

uint32_t xcrc32 (const unsigned char *buf, int len, uint32_t init);