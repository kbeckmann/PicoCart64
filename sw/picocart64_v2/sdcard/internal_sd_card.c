/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/structs/systick.h"

#include "ff.h" /* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "f_util.h"
#include "my_debug.h"
#include "rtc.h"
#include "hw_config.h"

#include "qspi_helper.h"
#include "internal_sd_card.h"
#include "psram.h"
#include "ringbuf.h"

#include "utils.h"

#include "flash_array.h"
#include "program_flash_array.h"

#include "FreeRTOS.h"
#include "task.h"

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
#define COMMAND_LOAD_ROM 0x6C // literally the l char
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
char sd_selected_rom_title[256];
volatile bool sd_is_busy = true;

// Variables used for signalling sd data send 
volatile bool waitingForRomLoad = false;
volatile bool sendDataReady = false;
volatile uint32_t sectorToSend = 0;
volatile uint32_t numSectorsToSend = 0;
volatile bool startRomLoad = false;
volatile bool romLoading = false;

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

void pc64_set_sd_rom_selection(char* titleBuffer, uint16_t len) {
    strcpy(sd_selected_rom_title, titleBuffer);
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

// Send command from MCU1 to MCU2 to start loading a rom
void pc64_send_load_new_rom_command() {
        // Block cart while waiting for data
    sd_is_busy = true;
    sendDataReady = false;
    romLoading = true;
    bufferIndex = 0;
    bufferByteIndex = 0;

    // Signal start
    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);

    // command
    uart_tx_program_putc(COMMAND_LOAD_ROM);

    // Send the title to load
    uart_tx_program_puts(sd_selected_rom_title);

    // Signal finish
    uart_tx_program_putc(COMMAND_FINISH);
    uart_tx_program_putc(COMMAND_FINISH2);
}

void load_selected_rom() {
    printf("Loading '%s'...\n", sd_selected_rom_title);
    load_new_rom(sd_selected_rom_title);
}

void load_new_rom(char* filename) {
    sd_is_busy = true;
    char buf[256];
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

    for(int i = 0; i < 10000; i++) { tight_loop_contents(); }

    // Set output enable (OE) to normal mode on all QSPI IO pins
	// qspi_enable();
    current_mcu_enable_demux(true);
    psram_set_cs(3); // Use the PSRAM chip
    program_connect_internal_flash();
    program_flash_exit_xip();

	int len = 0;
	int total = 0;
	uint64_t t0 = to_us_since_boot(get_absolute_time());
    int currentPSRAMChip = 3;
	do {
        fr = f_read(&fil, buf, sizeof(buf), &len);
        program_write_buf(total, buf, len);
		total += len;

        int newChip = psram_addr_to_chip(total);
        if (newChip != currentPSRAMChip && newChip <= MAX_MEMORY_ARRAY_CHIP_INDEX) {
            printf("Changing psram chip. Was: %d, now: %d\n", currentPSRAMChip, newChip);
            printf("Total bytes: %d. Bytes remaining = %ld\n", total, (filinfo.fsize - total));
            currentPSRAMChip = newChip;
            psram_set_cs(currentPSRAMChip); // Switch the PSRAM chip
        }

	} while (len > 0); //007C8240 just lets us cut off some empty space
	uint64_t t1 = to_us_since_boot(get_absolute_time());
	uint32_t delta = (t1 - t0) / 1000;
	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	printf("Read %d bytes and programmed PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	fr = f_close(&fil);
	if (FR_OK != fr) {
		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	printf("---- read file done -----\n\n\n");


    // Set back to starting PSRAM chip to read a few bytes
    psram_set_cs(3); // Use the PSRAM chip
    
    // program_flash_read_data(0, buf, 32);
    // for(int i = 0; i < 16; i++) {
    //     printf("%02x\n", buf[i]);
    // }

    // Send command to enter quad mode
    program_flash_do_cmd(0x35, NULL, NULL, 0);
    program_flash_flush_cache();

    // Now enable xip and try to read
    program_flash_enter_cmd_xip(true);
    psram_set_cs(3); // Use the PSRAM chip

    printf("\n\nWITH qspi_enter_cmd_xip\n");
    // volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    volatile uint32_t *ptr = (volatile uint32_t *)0x13000000;
    uint32_t cycleCountStart = 0;
    uint32_t totalTime = 0;
    int psram_csToggleTime = 0;
    int total_memoryAccessTime = 0;
    int totalReadTime = 0;
    for (int i = 0; i < 128; i++) {
        uint32_t modifiedAddress = i;
        uint32_t startTime_us = time_us_32();
        uint32_t word = ptr[modifiedAddress];
        totalReadTime += time_us_32() - startTime_us;

        if (i < 16) { // only print the first 16 words
            printf("PSRAM-MCU2[%d]: %08x\n",i, word);
        }
    }

    printf("\n128 32bit reads @ 0x10000000 reads took %d us\n", totalReadTime);

    // Send exit quad mode command
    // TODO exit quad mode on every chip
    exitQuadMode();

    return;

    // Now turn off the hardware
    current_mcu_enable_demux(false);
    ssi_hw->ssienr = 0;

    qspi_disable();
    printf("Rom Loaded, MCU2 qspi: OFF, sending mcu1 rom loaded command\n");

    // Let MCU1 know that we are finished
    // Signal start
    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);

    // command
    uart_tx_program_putc(0xAA);

    // Signal finish
    uart_tx_program_putc(COMMAND_FINISH);
    uart_tx_program_putc(COMMAND_FINISH2);
}

// MCU listens for other MCU commands and will respond accordingly
int startIndex = 0;
bool mayHaveStart = false;
bool mayHaveFinish = false;
bool receivingData = false;
unsigned char mcu2_cmd_buffer[64]; // TODO rename
int echoIndex = 0;
void mcu1_process_rx_buffer() {
    while (rx_uart_buffer_has_data()) {
        uint8_t value = rx_uart_buffer_get();
        
        #if MCU1_ECHO_RECEIVED_DATA == 1
        uart_tx_program_putc(value);
        #endif

        if (romLoading) {
            // Echo the chars
            uart_tx_program_putc(value);

            if (value == COMMAND_START) {
                mayHaveStart = true;
            } else if (value == COMMAND_START2 && mayHaveStart) {
                receivingData = true;
            } else if (value == COMMAND_FINISH && receivingData) {
                mayHaveFinish = true;
            } else if (value == COMMAND_FINISH2 && receivingData) {
                receivingData = false;
            } else if (receivingData && !mayHaveFinish) {
                mcu2_cmd_buffer[bufferIndex] = value;
                bufferIndex++;
            }

            if (mayHaveFinish && !receivingData) {
                char command = mcu2_cmd_buffer[0];
                // TODO check the command? 
                // May not be needed

                romLoading = false; // signal that the rom is finished loading
                sendDataReady = true;
                
                // Reset state 
                bufferIndex = 0;
                mayHaveFinish = false;
                mayHaveStart = false;
                receivingData = false;
            }

        } else {
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

            if (bufferByteIndex >= 512) {
                sendDataReady = true;
            }
        }
    }
}

void mcu2_process_rx_buffer() {
    while (rx_uart_buffer_has_data()) {
        char ch = rx_uart_buffer_get();
        
        printf("%02x ", ch);

        if (ch == COMMAND_START) {
            mayHaveStart = true;
        } else if (ch == COMMAND_START2 && mayHaveStart) {
            receivingData = true;
        } else if (ch == COMMAND_FINISH && receivingData) {
            mayHaveFinish = true;
        } else if (ch == COMMAND_FINISH2 && receivingData) {
            receivingData = false;
        } else if (receivingData && !mayHaveFinish) {
            ((uint8_t*)(pc64_uart_tx_buf))[bufferIndex] = ch;
            bufferIndex++;
        }

        if (mayHaveFinish && !receivingData) {
            // end of command
            // process what was sent
            uint8_t* buffer = (uint8_t*)pc64_uart_tx_buf; // cast to char array
            char command = buffer[0];

            if (command == COMMAND_SD_READ) {
                uint32_t sector_front =(buffer[1] << 24) | (buffer[2] << 16) | (buffer[3] << 8) | buffer[4];
                uint32_t sector_back = (buffer[5] << 24) | (buffer[6] << 16) | (buffer[7] << 8) | buffer[8];
                //uint64_t sector = ((uint64_t)sector_front) << 32 | sector_back;
                volatile uint32_t sectorCount = (buffer[9] << 24) | (buffer[10] << 16) | (buffer[11] << 8) | buffer[12];
                sectorToSend = sector_back;
                numSectorsToSend = 1;
                sendDataReady = true;
                
            } else if (command == COMMAND_LOAD_ROM) {
                sprintf(sd_selected_rom_title, "%s", buffer+1);
                startRomLoad = true;

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
            if (echoIndex >= 32) {
                printf("\n");
                echoIndex = 0;
            }
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

#define RUN_QSPI_PERMUTATION_TESTS 0
#define ROM_WRITE_OFFSET 0

#if LOAD_TO_PSRAM_ARRAY == 0
uint8_t buf[FLASH_PAGE_SIZE];
void __no_inline_not_in_flash_func(load_rom)(const char* filename) {
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
    printf("DONE!\nStarting rom->flash loading process...\n");

    printf("Enable demux.\n");
    current_mcu_enable_demux(true);
    
    printf("Set CS pad address to 2.\n");
    psram_set_cs(2);
    printf("DONE!\n");

#if ERASE_AND_WRITE_TO_FLASH_ARRAY == 0
    printf("Conecting internal flash.\n");
    program_connect_internal_flash();
    printf("Exiting flash xip.\n");
    program_flash_exit_xip();

    int len = 0;
	int total = 0;
    uint32_t numBlocksToErase = (filinfo.fsize / FLASH_SECTOR_SIZE) + 1;
    uint32_t erasedBlocks = 0;
    printf("Erasing %d %dKB blocks flash...\n", numBlocksToErase, FLASH_SECTOR_SIZE/1024);
    do {
        uint32_t addressToErase = erasedBlocks * FLASH_SECTOR_SIZE;
        picocart_flash_range_erase(addressToErase, FLASH_SECTOR_SIZE);
        numBlocksToErase--;
        erasedBlocks++;
    } while(numBlocksToErase > 0);

    printf("Erased %d blocks.\nProgramming flash...\n", erasedBlocks);

	uint64_t t0 = to_us_since_boot(get_absolute_time());
	do {        
        fr = f_read(&fil, buf, sizeof(buf), &len);
        picocart_flash_range_program(total, buf, len);
		total += len;
	} while (len > 0);
	uint64_t t1 = to_us_since_boot(get_absolute_time());
	uint32_t delta = (t1 - t0) / 1000;
	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	printf("Read %d bytes and programmed FLASH in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	fr = f_close(&fil);
	if (FR_OK != fr) {
		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	printf("---- read file done -----\n\n\n");

    printf("FLASH_READ_DATA before reenabling xip\n");
    program_flash_read_data(0, buf, 256);
    for(int i = 0; i < 16; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    program_flash_flush_cache();
    picocart_flash_enable_xip_via_boot2();
    //picocart_boot2_enable();
    if (isBoot2Valid) {
        printf("\n\nBoot2 is copied!\n\n");
    } else {
        printf("\n\n!!!BOOT2 NOT COPIED!!!\n\n");
    }
#else
    program_connect_internal_flash();
    program_flash_exit_xip();
    program_flash_flush_cache();
    picocart_flash_enable_xip_via_boot2();
#endif

    // If xip is renabled, this won't work anymore
    // printf("FLASH_READ_DATA\n");
    // program_flash_read_data(0, buf, 256);
    // for(int i = 0; i < 16; i++) {
    //     printf("%02x ", buf[i]);
    // }
    // printf("\n");

    // printf("PTR (xip mode?) reads\n");
    // volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    uint32_t cycleCountStart = 0;
    int totalReadTime = 0;
    // for (int i = 0; i < 128; i++) {
    //     uint32_t modifiedAddress = i;
        
    //     uint32_t startTime_us = time_us_32();
    //     psram_set_cs(2);
    //     uint32_t word = ptr[modifiedAddress];
    //     psram_set_cs(0);
    //     totalReadTime += time_us_32() - startTime_us;

    //     if (i < 16) { // only print the first 16 words
    //         // if (i == 16) { printf("\n"); }
    //         printf("FLASH-MCU2[%d]: %08x\n",i, word);
    //     }
    // }

    // printf("\n128 32bit reads @ 0x10000000 reads took %d us\n", totalReadTime);

    printf("PTR (xip mode?) reads\n");
    volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;
	printf("Access using 16bit pointer at [0x10000000]\n");
	uint32_t totalTime = 0;
	for(int i = 0; i < 4096; i+=2) {
		uint32_t now = time_us_32();	
		uint16_t word = ptr16[i >> 1];
		totalTime += time_us_32() - now;

		if (i < 64) {
			if (i % 8 == 0) {
				printf("\n%08x: ", i);
				
			}
			printf("%04x ", word);
		}
	}
	float elapsed_time_s = 1e-6f * totalTime;
	printf("\nxip access for 4kB via 16bit pointer took %d us. %.3f MB/s\n", totalTime, ((4096 / 1e6f) / elapsed_time_s));

    picocart_boot2_enable();
    printf("\nAccess with boot2 using 16bit pointer at [0x10000000]\n");
	totalTime = 0;
	for(int i = 0; i < 4096; i+=2) {
		uint32_t now = time_us_32();	
		uint16_t word = ptr16[i >> 1];
		totalTime += time_us_32() - now;

		if (i < 64) {
			if (i % 8 == 0) {
				printf("\n%08x: ", i);
				
			}
			printf("%04x ", word);
		}
	}
	elapsed_time_s = 1e-6f * totalTime;
	printf("\nxip(via boot2) access for 4kB via 16bit pointer took %d us. %.3f MB/s\n", totalTime, ((4096 / 1e6f) / elapsed_time_s));

}

#elif LOAD_TO_PSRAM_ARRAY == 1
char buf[1024 / 2 / 2 / 2 / 2];
void __no_inline_not_in_flash_func(load_rom)(const char *filename) {
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

    for(int i = 0; i < 10000; i++) { tight_loop_contents(); }

    // Set output enable (OE) to normal mode on all QSPI IO pins except SS
	qspi_enable();

	int len = 0;
	int total = 0;
	uint64_t t0 = to_us_since_boot(get_absolute_time());
	do {
        #if ROM_WRITE_OFFSET == 1
        // Some dumb hack to offset the values so reads would work in fast read mode
        // But it seems to keep the extra zeros when I thought they would be chopped off
        // 64 / 4 = 16
        // pad each one empty before every 3 bytes
        // 64 - 16 = 48, so each qspi write will write 48 bytes of actual data
        char offset_buf[128];
		fr = f_read(&fil, buf, sizeof(buf), &len);
        int index = 0;
        int bufIndex = 0;
        do {
            if (index % 4 == 3 || index % 4 == 0) {
                offset_buf[index] = 0;
            } else {
                offset_buf[index] = buf[bufIndex];
                bufIndex++;
            }
            
            index++;
        } while (index < 64);

        qspi_write(total, offset_buf, len);

        #else
        
        fr = f_read(&fil, buf, sizeof(buf), &len);
		qspi_write(total, buf, len);

        #endif

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

    systick_hw->csr = 0x5;
    systick_hw->rvr = 0x00FFFFFF;

    qspi_enable();
    // printf("Read from PSRAM via qspi_read1\n");
    // char buf2[4096];
    uint32_t totalTime = 0;
    // uint32_t startTimeReadFunction = time_us_32();
    // qspi_read(0, buf2, 4096);
    // totalTime = time_us_32() - startTimeReadFunction;
    // printf("00000000: ");
    // for(int i = 0; i < 16; i++) {
    //     if (i % 8 == 0 && i != 0) {
    //         printf("\n%08x: ", i);
    //     }
    //     printf("%02x ", buf2[i]);
    // }
    // printf("\n128 32bit (4096 bytes) reads with qspi_read took %dus\n", totalTime);


////////////////////////////////////////////////////////////////////////////////////////////////
    // qspi_enter_cmd_xip();
    // qspi_init_qspi(true);
    // printf("\n\nRead with XIP in QSPI(real quad) mode\n");
    // volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;//0x10000000;
    // printf("Access using 16bit pointer at [0x10000000]\n");
    // totalTime = 0;
    // for(int i = 0; i < 4096; i+=2) {
    //     uint32_t now = time_us_32();

    //     sio_hw->gpio_out = 0x00040000;
    //     uint16_t word = ptr16[i >> 1];
    //     sio_hw->gpio_out = 0x00000000;
        
    //     totalTime += time_us_32() - now;

    //     if (i < 64) {
    //         if (i % 8 == 0) {
    //             printf("\n%08x: ", i);
                
    //         }
    //         printf("%02x %02x ", (word & 0x00FF), word >> 8);
    //     }
    // }
    // printf("\nxip access (using raw gpio mask) for 4k Bytes via 16bit pointer took %d us\n\n", totalTime);

    // printf("\nFAST READ  - DMA TRANSFER\n");
    // // qspi_enter_cmd_xip();
    // uint32_t dmaBuffer[128];
    // uint32_t startTime_us = time_us_32();
    // psram_set_cs(DEBUG_CS_CHIP_USE);
    // qspi_flash_bulk_read(0x0B, dmaBuffer, 0, 128, 0);
    // psram_set_cs(0);
    // uint32_t dma_totalTime = time_us_32() - startTime_us;
    // for(int i = 0; i < 16; i++) {
    //     printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    // }
    // printf("FAST READ - DMA read 128 32bit reads in %d us\n", dma_totalTime);
////////////////////////////////////////////////////////////////////////////////////////////////


    // Now see if regular reads work
    // qspi_enter_cmd_xip();
    // printf("\n\nWITH qspi_enter_cmd_xip\n");
    // ptr = (volatile uint32_t *)0x10000000;
    // uint32_t cycleCountStart = 0;
    // int psram_csToggleTime = 0;
    // int total_memoryAccessTime = 0;
    // int totalReadTime = 0;
    // for (int i = 0; i < 128; i++) {
    //     uint32_t modifiedAddress = i;
        
    //     uint32_t startTime_us = time_us_32();
    //     psram_set_cs(DEBUG_CS_CHIP_USE);
    //     uint32_t word = ptr[modifiedAddress];
    //     psram_set_cs(0);

    //     totalReadTime += time_us_32() - startTime_us;
    //     if (i < 16) { // only print the first 16 words
    //         printf("PSRAM-MCU2[%d]: %08x\n",i, word);
    //     }
    // }

    // printf("\n128 32bit reads @ 0x10000000 reads took %d us\n", totalReadTime);
    // totalTime = 0;

    // This test doesn't quite work right, the data isn't all right
    // volatile uint16_t *ptr_16 = (volatile uint16_t *)0x10000000;
    // for (int i = 0; i < 256;) {
    //     uint32_t startTime_us = time_us_32();
    //     psram_set_cs(DEBUG_CS_CHIP_USE));
    //     uint32_t word1 = ptr_16[i];
    //     uint32_t word2 = ptr_16[i+1];
    //     psram_set_cs(0);
    //     totalTime += time_us_32() - startTime_us;
    //     if (i < 32) { // only print the first 32 words
    //         printf("PSRAM-MCU2[%d]: %04x %04x\n",i, word1, word2);
    //     }
    //     i += 2;
    // }

    // printf("\n256 16bit reads @ 0x10000000 reads took %dus\n", totalTime);
    totalTime = 0;

    #if RUN_QSPI_PERMUTATION_TESTS == 1

    // read commands: read, fast read, fast read quad
    // uint8_t cmds[] = { 0x03, 0x0B, 0xEB };
    // int waitCycles[] = {0, 8, 6};

    uint8_t cmds[] = { 0x0B, 0xEB };
    int waitCycles[] = {0, 4, 6, 8};
    int baudDividers[] = {2};
    int dataFrameSizes[] = { 31 };// 3, 7, 15,... 31 is the only on that returns sensible data
    int addr_ls[] = { 6, 8 };

    int cmdSize = 2;
    int waitCycleSize = 4;
    int baudDividerSize = 1;
    int addr_lSize = 2;


    /* Of the fast read quad, this was the closest
    // It's consistenly off and returns this data everytime. The data is RIGHT but it's not in the right positions
    // so 00040080 is actually at index 2 and the next value 54203436 doesn't show up until index 10
    cmd:eb, frf:02, trans_type:01, dfs:31, waitCycles: 6, baudDivider:2
    00040080 54203436 00588040 0400b4af 30a40c3c 00000000 fdff2016 000089ad FAILED -- Finished in 190us


    And of the 0b reads, these are missing the first byte at each index
    cmd:0b, frf:00, trans_type:01, waitCycles: 6, baudDivider:2
    12378000 00000000 04008000 14000000 8f45d700 b3455200 FAILED -- Finished in 254us
    */

    uint32_t correct_data[] = { 0x40123780, 0x0f000000, 0x00040080, 0x44140000, 0x248f45d7, 0x5fb34552, 0x00000000, 0x00000000 };

    // loop through each command
    for(int c_i = 0; c_i < cmdSize; c_i++) {
        uint8_t cmd = cmds[c_i];

        // And each wait cycle
        for(int wc_i = 0; wc_i < waitCycleSize; wc_i++) {
            int numWaitCycles = waitCycles[wc_i];
            
            // And finally each baud divider
            for(int bd_i = 0; bd_i < baudDividerSize; bd_i++) {
                int baudDivider = baudDividers[bd_i];

                for(int dfs_i = 0; dfs_i < 1; dfs_i++) {
                    int dfs = dataFrameSizes[dfs_i];

                    for(int qa = 0; qa < 2; qa++) {
                        for(int qd = 0; qd < 2; qd++) {
                            bool quad_address = qa;
                            bool quad_data = qd;
                            for(int addrl_i = 0; addrl_i < addr_lSize; addrl_i++) {
                                int addr_l = addr_ls[addrl_i];
                                qspi_enter_cmd_xip_with_params(cmd, quad_address, quad_data, dfs, addr_l, numWaitCycles, baudDivider);
                                
                                totalTime = 0;
                                bool errors = false;
                                // The DMA reads will stall out if there is a configuration error
                                // uint32_t p_buffer[128];
                                // uint32_t startTime_us = time_us_32();
                                // psram_set_cs(DEBUG_CS_CHIP_USE));
                                // qspi_flash_bulk_read(cmd, p_buffer, 0, 128, 0);
                                // psram_set_cs(0);
                                // totalTime += time_us_32() - startTime_us;

                                // for (int i = 0; i < 128; i++) {
                                //     uint32_t word = p_buffer[i];
                                //     if (i < 16) {
                                //         printf("%08x ", word);
                                //         if (correct_data[i] != word) {
                                //             errors = true;
                                //         }
                                //     }
                                // }

                                // actually do the reads now
                                volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
                                for (int i = 0; i < 128; i++) {
                                    uint32_t startTime_us = time_us_32();
                                    uint32_t modifiedAddress = i;
                                    psram_set_cs(DEBUG_CS_CHIP_USE);
                                    uint32_t word = ptr[modifiedAddress];
                                    psram_set_cs(0);
                                    totalTime += time_us_32() - startTime_us;
                                    if (i < 16) { // only check the first 8 words
                                        printf("%08x ", word);
                                        if(correct_data[i] != word) {
                                            //printf("|| Found: %08x, Expected: %08x, ", word, correct_data[i]);
                                            errors = true;
                                        }
                                    }
                                }

                                printf("%s -- Finished in %dus\n\n", !errors ? "SUCCESS!" : "FAILED", totalTime);
                            }
                        }
                    }
                }
            }
        }
    }

    #endif

    // Try a DMA read
    // printf("\nDMA TRANSFER\n");
    // qspi_enter_cmd_xip();
    // uint32_t dmaBuffer[128];
    // uint32_t startTime_us = time_us_32();
    // psram_set_cs(DEBUG_CS_CHIP_USE);
    // qspi_flash_bulk_read(0xEB, dmaBuffer, 0, 128, 0);
    // psram_set_cs(0);
    // uint32_t dma_totalTime = time_us_32() - startTime_us;
    // for(int i = 0; i < 16; i++) {
    //     printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    // }
    // printf("DMA read 128 32bit reads in %d us\n", dma_totalTime);

	// Release the qspi control
    qspi_disable();
}
#endif