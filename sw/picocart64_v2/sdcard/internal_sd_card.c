/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "ff.h" /* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "f_util.h"
#include "my_debug.h"
#include "rtc.h"
#include "hw_config.h"

#include "psram_inline.h"
#include "internal_sd_card.h"
#include "pio_uart/pio_uart.h"

#define SD_CARD_RX_READ_DEBUG 0

#define REGISTER_SD_COMMAND 0x0 // 1 byte, r/w
#define REGISTER_SD_READ_SECTOR 0x1 // 4 bytes
#define REGISTER_SD_READ_SECTOR_COUNT 0x5 // 4 bytes
#define COMMAND_START    0xDE
#define COMMAND_START2   0xAD
#define COMMAND_FINISH   0xBE
#define COMMAND_FINISH2  0xEF
#define COMMAND_SD_READ  0x72 // literally the r char
#define COMMAND_SD_WRITE 0x77 // literally the w char
#define DISK_READ_BUFFER_SIZE 512

int PC64_MCU_ID = -1;

volatile uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

volatile uint16_t sd_sector_registers[4];
volatile uint64_t sd_read_sector_start;
volatile uint32_t sd_read_sector_count;
volatile char sd_selected_rom_title[256];
volatile bool sd_is_busy = true;

// Variables used to signal sd data send from mcu2 -> mcu1 and the data that needs to be sent
volatile bool sendDataReady = false;
volatile uint64_t sectorToSend = 0;
volatile uint32_t numSectorsToSend = 0;

// sd_card_t *pSD; // Reference to the SD card object. Only valid on MCU2

void pc64_set_sd_read_sector(uint64_t sector) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set read sector = %ld", sector);
    #endif
    sd_read_sector_start = sector;
}

void pc64_set_sd_read_sector_part(uint index, uint16_t value) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set read sector part %d = %d", index, value);
    #endif
    sd_sector_registers[index] = value;
    // Assume we have set the other sectors as since this is the final piece
    // We can set the sector we want to actually read
    if (index == 3) {
        pc64_set_sd_read_sector(((uint64_t)(sd_sector_registers[0]) << 48 | ((uint64_t)sd_sector_registers[1]) << 32 | sd_sector_registers[2] << 16 | sd_sector_registers[3]));
    }
}

void pc64_set_sd_read_sector_count(uint32_t count) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set sector count = %d", count);
    #endif
    sd_read_sector_count = count;
    // sd_read_sector_count = 1;
}

void pc64_set_sd_rom_selection(char* title) {
    for (int i = 0; i < 256; i++) {
        sd_selected_rom_title[i] = title[i];
    }
}

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

// MCU1 will rx data from MCU2, this is SD card data
volatile int bufferIndex = 0;
volatile bool mightBeFinished = false;
void on_uart_rx_mcu1() {
    while (uart_rx_program_is_readable()) {
        char ch = uart_rx_program_getc();

        pc64_uart_tx_buf[bufferIndex++] = ch;

        if (ch == COMMAND_FINISH && bufferIndex >= 500) {
            mightBeFinished = true;
        } else if (mightBeFinished && ch == COMMAND_FINISH2) {
            bufferIndex = 0;
            sd_is_busy = false;
            mightBeFinished = false;
        }

        // Once we have recieved sd card data for the block, mark it as not busy
        // if (bufferIndex >= SD_CARD_SECTOR_SIZE) {
        //     bufferIndex = 0;
        //     sd_is_busy = false;
        // }
    }
}

// MCU2 listens for MCU1 commands and will respond accordingly
int startIndex = 0;
bool mayHaveStart = false;
bool mayHaveFinish = false;
bool receivingData = false;
unsigned char mcu2_cmd_buffer[64];
void on_uart_rx_mcu2() {
    // read
    while (uart_rx_program_is_readable()) {
        char ch = uart_rx_program_getc();
        
        if (ch == COMMAND_START) {
            mayHaveStart = true;
        } else if (ch == COMMAND_START2 && mayHaveStart) {
            receivingData = true;
        } else if (ch == COMMAND_FINISH && receivingData) {
            mayHaveFinish = true;
        } else if (ch == COMMAND_FINISH2 && receivingData) {
            receivingData = false;
        } else if (receivingData && !mayHaveFinish) {
            mcu2_cmd_buffer[bufferIndex] = ch;
            bufferIndex++;
        }

        if (mayHaveFinish && !receivingData) {
            // end of command
            // process what was sent
            char command = mcu2_cmd_buffer[0];
            uint32_t sector_front =(mcu2_cmd_buffer[1] << 24) | (mcu2_cmd_buffer[2] << 16) | (mcu2_cmd_buffer[3] << 8) | mcu2_cmd_buffer[4];
            uint32_t sector_back = (mcu2_cmd_buffer[5] << 24) | (mcu2_cmd_buffer[6] << 16) | (mcu2_cmd_buffer[7] << 8) | mcu2_cmd_buffer[8];
            uint64_t sector = ((uint64_t)sector_front) << 32 | sector_back;
            volatile uint32_t sectorCount = (mcu2_cmd_buffer[9] << 24) | (mcu2_cmd_buffer[10] << 16) | (mcu2_cmd_buffer[11] << 8) | mcu2_cmd_buffer[12];

            if (command == COMMAND_SD_READ) {
                sectorToSend = sector;
                numSectorsToSend = 1;
                sendDataReady = true;
                printf("SendDataReady, sectorToSend=%lx|%lx, numSectorsToSend=%x, actual sector count received=%x\n", sectorToSend, sector, numSectorsToSend, sectorCount);
            } else {
                // not supported yet
                printf("Unknown command: %x\n", command);
            }

            bufferIndex = 0;
            mayHaveFinish = false;
            mayHaveStart = false;
            receivingData = false;
        }

        if (bufferIndex-1 == 14) {
            bufferIndex = 0;
            printf("Error: Missing last byte\n");
        }
    }
}

BYTE diskReadBuffer[DISK_READ_BUFFER_SIZE*2];
// MCU2 will send data once it has the information it needs
void send_data(uint64_t sector, uint32_t sectorCount) {
    if (sectorCount == 0) {
        printf("Sector count was 0, setting to 1\n");
        sectorCount = 1;
    }

    printf("Sending data. Sector: %ld, Count: %d\n", sector, sectorCount);

    int loopCount = 0;
    do {
        loopCount++;
        // read 1 sector at a time
        // assume 512 byte sectors
        DRESULT dr = disk_read(0, diskReadBuffer, sector, 1);
        // DRESULT dr = sd_read_blocks(pSD, diskReadBuffer, sector, 1);
        if (dr != RES_OK) {
            printf("Error reading disk: %d\n", dr);
        } 
        // else {
        //     printf("Disk read at sector %ld successful!\n", sector);
        // }
        if (dr != RES_OK) {
            break;
        }
        sectorCount--;

        // Send sector worth of data
        for (uint diskBufferIndex = 0; diskBufferIndex < DISK_READ_BUFFER_SIZE; diskBufferIndex++) {
            // wait until uart is writable
            while (!uart_tx_program_is_writable()) {
                tight_loop_contents();
            }
            uart_tx_program_putc(diskReadBuffer[diskBufferIndex]);
        }
    // Repeat if we are reading more than 1 sector
    } while(sectorCount > 1);

    uart_tx_program_putc(COMMAND_FINISH);
    uart_tx_program_putc(COMMAND_FINISH2);    

    printf("Sent %ld sectors starting at Sector %d\n", loopCount, sector);

    printf("buffer: \n");
    for (uint diskBufferIndex = 0; diskBufferIndex < DISK_READ_BUFFER_SIZE; diskBufferIndex++) {
            printf("%x ", diskReadBuffer[diskBufferIndex]);
        }
    printf("\n");
}

void send_sd_card_data() {
    // Reset send data flag
    sendDataReady = false; 

    // Send the data over uart back to MCU1 so the rom can read it
    send_data(sectorToSend, numSectorsToSend);

    // Reset our other variables
    //sectorToSend = 0;
    //numSectorsToSend = 0;
}

// SD mount helper function
static sd_card_t *sd_get_by_name(const char *const name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name)) return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}
// SD mount helper function
static FATFS *sd_get_fs_by_name(const char *name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name)) return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

void mount_sd(void) {
    printf("Mounting SD Card\n");

    // // See FatFs - Generic FAT Filesystem Module, "Application Interface",
	// // http://elm-chan.org/fsw/ff/00index_e.html
	// pSD = sd_get_by_num(0);
	// FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
	// if (FR_OK != fr) {
	// 	panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
	// }

    // printf("SD Card mounted. Status: %d\n", fr);

    // Original code from test project. Might not need this more complicated version
    //const char *arg1 = strtok(NULL, " ");
    //if (!arg1) arg1 = sd_get_by_num(0)->pcName;
    const char *arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    if (pSD == NULL) {
        printf("Error getting sd card by name: %s\n", arg1);
    }
    pSD->mounted = true;
}