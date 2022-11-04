/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include "internal_sd_card.h"
#include "pio_uart/pio_uart.h"

uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];
static uint16_t sd_sector_registers[4];

static uint64_t sd_read_sector_start;
static uint32_t sd_read_sector_count;
static char sd_selected_rom_title[256];
static bool sd_is_busy = true;

void pc64_set_sd_read_sector(uint64_t sector) {
    sd_read_sector_start = sector;
}

void pc64_set_sd_read_sector_part(uint index, uint16_t value) {
    sd_sector_registers[index] = value;
    if (index == 3) {
        pc64_set_sd_read_sector(((uint64_t)(sd_sector_registers[0]) << 48 | ((uint64_t)sd_sector_registers[1]) << 32 | sd_sector_registers[2] << 16 | sd_sector_registers[3]));
    }
}

void pc64_set_sd_read_sector_count(uint32_t count) {
    //sd_read_sector_count = count;
    sd_read_sector_count = 1;
}

void pc64_set_sd_rom_selection(char* title) {
    for (int i = 0; i < 256; i++) {
        sd_selected_rom_title[i] = title[i];
    }
}

#define REGISTER_SD_COMMAND 0x0 // 1 byte, r/w
#define REGISTER_SD_READ_SECTOR 0x1 // 4 bytes
#define REGISTER_SD_READ_SECTOR_COUNT 0x5 // 4 bytes
#define COMMAND_START    0xDE
#define COMMAND_START2   0xAD
#define COMMAND_FINISH   0xBE
#define COMMAND_FINISH2  0xEF
#define COMMAND_SD_READ  0x72 // literally the r char
void pc64_send_sd_read_command(void) {
    // Block cart while waiting for data
    sd_is_busy = true;

    uint64_t sector = sd_read_sector_start;
    uint32_t sectorCount = 1;

    // Signal start
    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);

    // command
    uart_tx_program_putc(COMMAND_SD_READ);

    // sector
    uart_tx_program_putc((char)((sector & 0xFF00000000000000) >> 56));
    uart_tx_program_putc((char)((sector & 0x00FF000000000000) >> 48));
    uart_tx_program_putc((char)((sector & 0x0000FF0000000000) >> 40));
    uart_tx_program_putc((char)((sector & 0x000000FF00000000) >> 32));

    uart_tx_program_putc((char)((sector & 0x00000000FF000000) >> 24));
    uart_tx_program_putc((char)((sector & 0x0000000000FF0000) >> 16));
    uart_tx_program_putc((char)((sector & 0x000000000000FF00) >> 8));
    uart_tx_program_putc((char)(sector  & 0x00000000000000FF));

    // num sectors
    uart_tx_program_putc((char)((sectorCount & 0xFF000000) >> 24));
    uart_tx_program_putc((char)((sectorCount & 0x00FF0000) >> 16));
    uart_tx_program_putc((char)((sectorCount & 0x0000FF00) >> 8));
    uart_tx_program_putc((char)(sectorCount & 0x000000FF));

    // Signal finish
    uart_tx_program_putc(COMMAND_FINISH);
    uart_tx_program_putc(COMMAND_FINISH2);
}

bool is_sd_busy() {
    return sd_is_busy;
}

// Data is coming in from the uart!
static int bufferIndex = 0;
bool mightBeFinished = false;
void on_uart_rx(void) {
    while (uart_rx_program_is_readable()) {
        char ch = uart_rx_program_getc();
        if (ch == '\n') {
            printf("%c", ch);
        } else {
            printf("%x ", ch);
        }

        pc64_uart_tx_buf[bufferIndex++] = ch;

        if (ch == COMMAND_FINISH && bufferIndex > 500) {
            mightBeFinished = true;
        } else if (mightBeFinished && ch == COMMAND_FINISH2) {
            bufferIndex = 0;
            sd_is_busy = false;
            mightBeFinished = false;
        }

        // Once we have recieved sd card data for the block, mark it as not busy
        if (bufferIndex >= SD_CARD_SECTOR_SIZE) {
            bufferIndex = 0;
            sd_is_busy = false;
        }
    }
}
