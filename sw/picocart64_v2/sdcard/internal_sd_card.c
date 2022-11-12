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
#include "qspi_helper.h"
#include "internal_sd_card.h"
#include "ringbuf.h"


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

#define PRINT_BUFFER_AFTER_SEND 0
#define MCU1_ECHO_RECEIVED_DATA 0

int PC64_MCU_ID = -1;

volatile int bufferIndex = 0; // Used on MCU1 to track where to put the next byte received from MCU2
uint8_t lastBufferValue = 0;
int bufferByteIndex = 0;
volatile uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

volatile uint16_t sd_sector_registers[4];
volatile uint32_t sd_read_sector_start;
volatile uint32_t sd_read_sector_count;
volatile char sd_selected_rom_title[256];
volatile bool sd_is_busy = true;

// Variables used for signalling sd data send 
volatile bool sendDataReady = false;
volatile uint32_t sectorToSend = 0;
volatile uint32_t numSectorsToSend = 0;

// void pc64_set_sd_read_sector(uint64_t sector) {
//     #if SD_CARD_RX_READ_DEBUG == 1
//         printf("set read sector = %ld", sector);
//     #endif
//     sd_read_sector_start = sector;
// }

void pc64_set_sd_read_sector_part(int index, uint16_t value) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set read sector part %d = %d", index, value);
    #endif
    sd_sector_registers[index] = value;
    // Assume we have set the other sectors as since this is the final piece
    // We can set the sector we want to actually read
    // if (index == 3) {
    //     uint64_t s = ((uint64_t)(sd_sector_registers[0]) << 48 | ((uint64_t)sd_sector_registers[1]) << 32 | sd_sector_registers[2] << 16 | sd_sector_registers[3]);
    //     pc64_set_sd_read_sector(s);
    // }
}

void pc64_set_sd_read_sector_count(uint32_t count) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set sector count = %d", count);
    #endif
    sd_read_sector_count = count;
}

void pc64_set_sd_rom_selection(char* title) {
    for (int i = 0; i < 256; i++) {
        sd_selected_rom_title[i] = title[i];
    }
}

int tempSector = 0;
void pc64_send_sd_read_command(void) {
    // Block cart while waiting for data
    sd_is_busy = true;
    sendDataReady = false;
    bufferIndex = 0;
    bufferByteIndex = 0;

    uint32_t sector = (uint32_t)(sd_sector_registers[2] << 16 | sd_sector_registers[3]);
    uint32_t sectorCount = 1;

    // Signal start
    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);

    // command
    uart_tx_program_putc(COMMAND_SD_READ);

    // sector
    uart_tx_program_putc(0);
    uart_tx_program_putc(0);
    uart_tx_program_putc(0);
    uart_tx_program_putc(0);

    uart_tx_program_putc((char)((sector & 0xFF000000) >> 24));
    uart_tx_program_putc((char)((sector & 0x00FF0000) >> 16));
    uart_tx_program_putc((char)((sector & 0x0000FF00) >> 8));
    uart_tx_program_putc((char)(sector  & 0x000000FF));

    // num sectors
    uart_tx_program_putc((char)((sectorCount & 0xFF000000) >> 24));
    uart_tx_program_putc((char)((sectorCount & 0x00FF0000) >> 16));
    uart_tx_program_putc((char)((sectorCount & 0x0000FF00) >> 8));
    uart_tx_program_putc((char)(sectorCount & 0x000000FF));

    // Signal finish
    uart_tx_program_putc(COMMAND_FINISH);
    uart_tx_program_putc(COMMAND_FINISH2);
}


void mcu1_process_rx_buffer() {
    while (rx_uart_buffer_has_data()) {
        uint8_t value = rx_uart_buffer_get();
        
        #if MCU1_ECHO_RECEIVED_DATA == 1
        uart_tx_program_putc(value);
        #endif

        // Combine two char values into a 16 bit value
        // Only increment bufferIndex when adding a value
        // else, store the ch into the holding field
        if (bufferByteIndex % 2 == 1) {
            uint16_t value16 = lastBufferValue << 8 | value;
            pc64_uart_tx_buf[bufferIndex] = value16;
            bufferIndex += 1;
        } else {
            lastBufferValue = value;
        }

        bufferByteIndex++;

        if (bufferByteIndex >= SD_CARD_SECTOR_SIZE) {
            sendDataReady = true;
            break;
        }
    }

    if (bufferByteIndex >= 512) {
        sendDataReady = true;
    }
}

// MCU2 listens for MCU1 commands and will respond accordingly
int startIndex = 0;
bool mayHaveStart = false;
bool mayHaveFinish = false;
bool receivingData = false;
unsigned char mcu2_cmd_buffer[64];
int echoIndex = 0;
void mcu2_process_rx_buffer() {
    while (rx_uart_buffer_has_data()) {
        char ch = rx_uart_buffer_get();
        
        printf("%02x ", ch);
        // if (ch == 0xAA) {
        //     printf("\n");
        // }

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
            //uint64_t sector = ((uint64_t)sector_front) << 32 | sector_back;
            volatile uint32_t sectorCount = (mcu2_cmd_buffer[9] << 24) | (mcu2_cmd_buffer[10] << 16) | (mcu2_cmd_buffer[11] << 8) | mcu2_cmd_buffer[12];

            if (command == COMMAND_SD_READ) {
                sectorToSend = sector_back;
                numSectorsToSend = 1;
                sendDataReady = true;
                //printf("Parsed! sector_front:%d sector_back: %d\n", sector_front, sector_back);
            } else {
                // not supported yet
                printf("\nUnknown command: %x\n", command);
            }

            bufferIndex = 0;
            mayHaveFinish = false;
            mayHaveStart = false;
            receivingData = false;
            echoIndex = 0;
        } else {
            echoIndex++;
            if (echoIndex >= 20) {
                printf("\n");
                echoIndex = 0;
            }
        }

        if (bufferIndex-1 == 14) {
            bufferIndex = 0;
            printf("\nError: Missing last byte\n");
        }
    }
}

BYTE diskReadBuffer[DISK_READ_BUFFER_SIZE];
// MCU2 will send data once it has the information it needs
void send_data(uint32_t sector, uint32_t sectorCount) {
    //printf("Sending data. Sector: %ld, Count: %d\n", sector, sectorCount);
    int loopCount = 0;
    do {
        loopCount++;

        DRESULT dr = disk_read(0, diskReadBuffer, (uint64_t)sector, 1);
        
        if (dr != RES_OK) {
            printf("Error reading disk: %d\n", dr);
        } 
        
        sectorCount--;

        // Send sector worth of data
        for (int diskBufferIndex = 0; diskBufferIndex < DISK_READ_BUFFER_SIZE; diskBufferIndex++) {
            // wait until uart is writable
            while (!uart_tx_program_is_writable()) {
                tight_loop_contents();
            }

            uart_tx_program_putc(diskReadBuffer[diskBufferIndex]);
        }

    // Repeat if we are reading more than 1 sector
    } while(sectorCount > 1);

    printf("Read Sector %u\n", sector);

    #if PRINT_BUFFER_AFTER_SEND == 1
        printf("buffer for sector: %ld\n", sector);
        for (uint diskBufferIndex = 0; diskBufferIndex < DISK_READ_BUFFER_SIZE; diskBufferIndex++) {
                printf("%02x ", diskReadBuffer[diskBufferIndex]);
            }
        printf("\n");
    #endif
}

void send_sd_card_data() {
    // Reset send data flag
    sendDataReady = false; 

    // Send the data over uart back to MCU1 so the rom can read it
    // uint64_t sector = sectorToSend;
    // uint32_t numSectors = 1;
    send_data(sectorToSend, 1);
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

char buf[1024 / 2 / 2 / 2 / 2];
// void __no_inline_not_in_flash_func(load_rom)(const char *filename)
// {
//     // pio_sm_set_enabled(pio1, 0, false);

//     qspi_enable();
//     // sleep_ms(10); // Apparently this kills the interrupts or everything,not really sure
//     qspi_enter_cmd_xip();
//     qspi_disable();

//     // pio_sm_set_enabled(pio1, 0, true);
// }

void __no_inline_not_in_flash_func(load_rom)(const char *filename)
{
    const uint CS_PIN_INDEX = 1;
    bool chipSelectEnabled = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    printf("Chip select enabled? %d\n", chipSelectEnabled);

	// Set output enable (OE) to normal mode on all QSPI IO pins except SS
	qspi_enable();

	// See FatFs - Generic FAT Filesystem Module, "Application Interface",
	// http://elm-chan.org/fsw/ff/00index_e.html
	sd_card_t *pSD = sd_get_by_num(0);
	FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
	if (FR_OK != fr) {
		panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
	}

	FIL fil;

	printf("\n\n---- read /%s -----\n", filename);

	fr = f_open(&fil, filename, FA_OPEN_EXISTING | FA_READ);
	if (FR_OK != fr && FR_EXIST != fr) {
		panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
	}

	FILINFO filinfo;
	fr = f_stat(filename, &filinfo);
	printf("%s [size=%llu]\n", filinfo.fname, filinfo.fsize);

	int len = 0;
	int total = 0;
	uint64_t t0 = to_us_since_boot(get_absolute_time());
	do {
		fr = f_read(&fil, buf, sizeof(buf), &len);
		qspi_write(total, buf, len);
		total += len;
	} while (len > 0);
	uint64_t t1 = to_us_since_boot(get_absolute_time());
	uint32_t delta = (t1 - t0) / 1000;
	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	printf("Read %d bytes and programmed PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	fr = f_close(&fil);
	if (FR_OK != fr) {
		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	printf("---- read file done -----\n\n\n");

	// printf("---- Verify file with PSRAM -----\n\n\n");

	//qspi_enter_cmd_xip();

    qspi_enter_cmd_xip();
    printf("WITH qspi_enter_cmd_xip\n");
    volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    for (int i = 0; i < 8; i++) {
        uint32_t modifiedAddress = i;
        psram_set_cs(1);
        uint32_t word = ptr[modifiedAddress];
        psram_set_cs(0);
        printf("PSRAM-MCU2[%d]: %08x\n",i, word);
    }

    printf("Read from PSRAM via qspi_read\n");
    char buf2[64];
    qspi_read(0, buf2, 64);
    for(int i = 0; i < 64; i++) {
        printf("%02x ", buf2[i]);
    }

	// fr = f_open(&fil, filename, FA_OPEN_EXISTING | FA_READ);
	// if (FR_OK != fr && FR_EXIST != fr) {
	// 	panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
	// }

	// fr = f_stat(filename, &filinfo);
	// printf("%s [size=%llu]\n", filinfo.fname, filinfo.fsize);

	// // {
	//  uint32_t *ptr = (uint32_t *) 0x10000000;

	//  for (int i = 0; i < 8 * 1024 / 4 * 2; i++) {
	//      psram_set_cs(1);
	//      uint32_t word = ptr[i];
	//      psram_set_cs(0);
	//      // printf("%08X ", word);
	//      if (word != i) {
	//          printf("ERR: %08X != %08X\n", word, i);
	//      }
	//  }

	// //  printf("----- halt ----\n");

	// //  while (1) {

	// //  }
	// // }

	// len = 0;
	// total = 0;
	// t0 = to_us_since_boot(get_absolute_time());
    // int numAddressesPrinted = 0;
	// do {
	// 	fr = f_read(&fil, buf, sizeof(buf), &len);

	// 	volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
	// 	uint32_t *buf32 = (uint32_t *) buf;
	// 	for (int i = 0; i < 8; i++) {
	// 		uint32_t address_32 = total / 4 + i;
	// 		uint32_t address = address_32 * 4;
	// 		psram_set_cs(psram_addr_to_chip(address));
	// 		uint32_t word = ptr[address_32];
	// 		uint32_t facit = buf32[i];
	// 		psram_set_cs(0);
	// 		// if (word != facit) {
	// 		// 	printf("diff @%08X + %d: %08X %08X\n", total, i * 4, word, facit);
	// 		// }

    //         if (numAddressesPrinted < 16) {
    //             numAddressesPrinted++;
    //             printf("%d, %08x | %08x -- addr=%d, addr32=%d\n", i, word, facit, address, address_32);
                
    //         }
	// 	}
	// 	total += len;
	// 	// printf("@%08X [len=%d]\n", total, len);

	// 	// if (total > 128) {
	// 	//  printf("Halting\n");
	// 	//  while (1) {
	// 	//  }
	// 	// }

    //     if (numAddressesPrinted >= 16) {
    //         break;
    //     }

	// } while (len > 0);
	// t1 = to_us_since_boot(get_absolute_time());
	// delta = (t1 - t0) / 1000;
	// kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	// printf("Verified %d bytes with PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	// //////////////////////////////
	// t0 = to_us_since_boot(get_absolute_time());
	// len = 64;
	// uint32_t totalRead = 0;
	// do {
	// 	volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
	// 	for (int i = 0; i < len / 4; i++) {
	// 		uint32_t address_32 = totalRead / 4 + i;
	// 		uint32_t address = address_32 * 4;
	// 		psram_set_cs(psram_addr_to_chip(address));
	// 		uint32_t word = ptr[address_32];
	// 		// uint32_t facit = buf32[i];
	// 		psram_set_cs(0);
	// 	}
	// 	totalRead += len;
	// } while (totalRead <= total);
	// t1 = to_us_since_boot(get_absolute_time());
	// delta = (t1 - t0) / 1000;
	// kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	// printf("Reread %d bytes from PSRAM in %d ms (%d kB/s)\n\n\n", totalRead, delta, kBps);
	// //////////////////////////////

	// fr = f_close(&fil);
	// if (FR_OK != fr) {
	// 	printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	// }

	// printf("---- Verify file with PSRAM Done -----\n\n\n");

	// f_unmount(pSD->pcName);

	// Release the qspi control
    qspi_disable();
}

void test_read_from_psram() {
    printf("PSRAM test read\n");
    qspi_enable();
    qspi_enter_cmd_xip();

    volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    for(int i = 0; i < 4; i++) {
        uint32_t address_32 = i;
        psram_set_cs(1);
        uint32_t word = ptr[address_32];
        psram_set_cs(0);

        printf("PSRAM-MCU2[%d] = %08x\n", address_32, word);
    }
    qspi_disable();
}