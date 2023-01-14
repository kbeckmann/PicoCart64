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
#define MCU2_PRINT_UART 1

int PC64_MCU_ID = -1;

volatile int bufferIndex = 0; // Used on MCU1 to track where to put the next byte received from MCU2
uint8_t lastBufferValue = 0;
int bufferByteIndex = 0;
volatile uint16_t pc64_uart_tx_buf[PC64_BASE_ADDRESS_LENGTH];

volatile uint32_t sd_sector_registers[4];
volatile uint32_t sd_sector_count_registers[2];
volatile uint32_t sd_read_sector_start;
volatile uint32_t sd_read_sector_count;
char sd_selected_rom_title[256];
uint32_t sd_selected_title_length = 0;
volatile bool sd_is_busy = false;

// Variables used for signalling sd data send 
volatile bool waitingForRomLoad = false;
volatile bool sendDataReady = false;
volatile uint32_t sectorToSendRegisters[2];
volatile uint32_t numSectorsToSend = 0;
volatile bool startRomLoad = false;
volatile bool romLoading = false;


void pc64_set_sd_read_sector_part(int index, uint32_t value) {
    #if SD_CARD_RX_READ_DEBUG == 1
        printf("set read sector part %d = %d", index, value);
    #endif
    sd_sector_registers[index] = value;
}

void pc64_set_sd_read_sector_count(int index, uint32_t count) {
    sd_sector_count_registers[index] = count; 
}

void pc64_set_sd_rom_selection(char* titleBuffer, uint32_t len) {
    sd_selected_title_length = len;
    strcpy(sd_selected_rom_title, titleBuffer);
}

int tempSector = 0;
void pc64_send_sd_read_command(void) {
    // Block cart while waiting for data
    sd_is_busy = true;
    sendDataReady = false;
    bufferIndex = 0;
    bufferByteIndex = 0;
    uint32_t sectorCount = 1;

    // Signal start
    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);

    // command
    uart_tx_program_putc(COMMAND_SD_READ);

    // 12 bytes to read
    uart_tx_program_putc(0);
    uart_tx_program_putc(12);

    // sector (top bytes)
    uart_tx_program_putc((char)(sd_sector_registers[0] >> 24));
    uart_tx_program_putc((char)(sd_sector_registers[0] >> 16));
    uart_tx_program_putc((char)(sd_sector_registers[1] >> 24));
    uart_tx_program_putc((char)(sd_sector_registers[1] >> 16));

    // sector (bottom bytes)
    uart_tx_program_putc((char)(sd_sector_registers[2] >> 24));
    uart_tx_program_putc((char)(sd_sector_registers[2] >> 16));
    uart_tx_program_putc((char)(sd_sector_registers[3] >> 24));
    uart_tx_program_putc((char)(sd_sector_registers[3] >> 16));

    // num sectors
    uart_tx_program_putc((char)((sectorCount & 0xFF000000) >> 24));
    uart_tx_program_putc((char)((sectorCount & 0x00FF0000) >> 16));
    uart_tx_program_putc((char)((sectorCount & 0x0000FF00) >> 8));
    uart_tx_program_putc((char) (sectorCount & 0x000000FF));
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

    uint16_t numBytes = strlen(sd_selected_rom_title);
    uart_tx_program_putc(numBytes >> 8);
    uart_tx_program_putc(numBytes);

    // Send the title to load
    uart_tx_program_puts(sd_selected_rom_title);
}

void load_selected_rom() {
    printf("Loading '%s'...\n", sd_selected_rom_title);
    load_new_rom(sd_selected_rom_title);
}

void load_new_rom(char* filename) {
    sd_is_busy = true;
    char buf[512];
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

    current_mcu_enable_demux(true);
    psram_set_cs(START_ROM_LOAD_CHIP_INDEX); // Use the PSRAM chip
    program_connect_internal_flash();
    program_flash_exit_xip();

	int len = 0;
	int total = 0;
	uint64_t t0 = to_us_since_boot(get_absolute_time());
    int currentPSRAMChip = START_ROM_LOAD_CHIP_INDEX;

	// do {
    //     fr = f_read(&fil, buf, sizeof(buf), &len);
    //     uint32_t addr = total - ((currentPSRAMChip - START_ROM_LOAD_CHIP_INDEX) * PSRAM_CHIP_CAPACITY_BYTES);
    //     program_write_buf(addr, buf, len);
	// 	total += len;

    //     int newChip = psram_addr_to_chip(total);
    //     if (newChip != currentPSRAMChip && newChip <= MAX_MEMORY_ARRAY_CHIP_INDEX) {
    //         printf("Changing memory array chip. Was: %d, now: %d\n", currentPSRAMChip, newChip);
    //         printf("Total bytes: %d. Bytes remaining = %ld\n", total, (filinfo.fsize - total));
    //         currentPSRAMChip = newChip;
    //         psram_set_cs(currentPSRAMChip); // Switch the PSRAM chip
    //     }

	// } while (len > 0);
	// uint64_t t1 = to_us_since_boot(get_absolute_time());
	// uint32_t delta = (t1 - t0) / 1000;
	// uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	// printf("Read %d bytes and programmed PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	fr = f_close(&fil);
	if (FR_OK != fr) {
		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	printf("---- read file done -----\n\n\n");

    psram_set_cs(START_ROM_LOAD_CHIP_INDEX);
    program_flash_do_cmd(0x35, NULL, NULL, 0);

    sleep_ms(100);

    psram_set_cs(START_ROM_LOAD_CHIP_INDEX+1);
    program_flash_do_cmd(0x35, NULL, NULL, 0);

    sleep_ms(100);

    psram_set_cs(START_ROM_LOAD_CHIP_INDEX+2);
    program_flash_do_cmd(0x35, NULL, NULL, 0);

    sleep_ms(100);

    psram_set_cs(START_ROM_LOAD_CHIP_INDEX+3);
    program_flash_do_cmd(0x35, NULL, NULL, 0);

    sleep_ms(100);

    program_flash_flush_cache();

    // Now enable xip and try to read
    for (int o = 0; o < 4; o++) {
        
        psram_set_cs(START_ROM_LOAD_CHIP_INDEX+o); // Use the PSRAM chip
        program_flash_enter_cmd_xip(true);

        printf("\n\nCheck data from U%u...\n", START_ROM_LOAD_CHIP_INDEX + o);
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
                printf("PSRAM-MCU2[%08x]: %08x\n",i * 4 + (o * 8 * 1024 * 1024), word);
            }
        }
        printf("\n128 32bit reads @ 0x13000000 reads took %d us\n", totalReadTime);

        exitQuadMode(); // exit quad mode once finished
        sleep_ms(100);
    }

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
bool isReadingCommandHeader = false;
uint8_t command_headerBufferIndex = 0;
uint16_t command_numBytesToRead = 0;
bool command_processBuffer = false;

#define COMMAND_HEADER_LENGTH 3
//COMMAND, NUM_BYTES_HIGH, NUM_BYTES_LOW
uint8_t commandHeaderBuffer[COMMAND_HEADER_LENGTH]; 

unsigned char mcu2_cmd_buffer[64]; // TODO rename
int echoIndex = 0;
void mcu1_process_rx_buffer() {
    while (rx_uart_buffer_has_data()) {
        uint8_t value = rx_uart_buffer_get();
        
        #if MCU1_ECHO_RECEIVED_DATA == 1
        // uart_tx_program_putc(value);
        #endif

        if (romLoading) {
            // Echo the chars
            // uart_tx_program_putc(value);

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
        
        #if MCU2_PRINT_UART == 1
        printf("%02x ", ch);
        #endif

        if (receivingData) {
            ((uint8_t*)(pc64_uart_tx_buf))[bufferIndex] = ch;
            bufferIndex++;

            if (bufferIndex >= command_numBytesToRead) {
                command_processBuffer = true;
                bufferIndex = 0;
            }
        } else if (isReadingCommandHeader) {
            commandHeaderBuffer[command_headerBufferIndex++] = ch;

            if (command_headerBufferIndex >= COMMAND_HEADER_LENGTH) {
                command_numBytesToRead = (commandHeaderBuffer[1] << 8) | commandHeaderBuffer[2];
                isReadingCommandHeader = false;
                receivingData = true;
                command_headerBufferIndex = 0;
            }
        } else if (ch == COMMAND_START && !receivingData) {
            mayHaveStart = true;
        } else if (ch == COMMAND_START2 && mayHaveStart && !receivingData) {
            isReadingCommandHeader = true;
        }

        if (command_processBuffer) {
            command_processBuffer = false;
            // process what was sent
            uint8_t* buffer = (uint8_t*)pc64_uart_tx_buf; // cast to char array
            char command = commandHeaderBuffer[0];

            if (command == COMMAND_SD_READ) {
                uint32_t sector_front =(buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
                uint32_t sector_back = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
                volatile uint32_t sectorCount = (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11];
                sectorToSendRegisters[0] = sector_front;
                sectorToSendRegisters[1] = sector_back;
                numSectorsToSend = 1;
                sendDataReady = true;
                
            } else if (command == COMMAND_LOAD_ROM) {
                sprintf(sd_selected_rom_title, "%s", buffer);
                startRomLoad = true;
                printf("nbtr: %u\n", command_numBytesToRead);

            } else {
                // not supported yet
                printf("\nUnknown command: %x\n", command);
            }

            bufferIndex = 0;
            mayHaveFinish = false;
            mayHaveStart = false;
            receivingData = false;
            command_numBytesToRead = 0;

            #if MCU2_PRINT_UART == 1
                echoIndex = 0;
                printf("\n");
            #endif
        } else {
            #if MCU2_PRINT_UART == 1
            echoIndex++;
            if (echoIndex >= 32) {
                printf("\n");
                echoIndex = 0;
            }
            #endif
        }
    }
}

BYTE diskReadBuffer[DISK_READ_BUFFER_SIZE];
// MCU2 will send data once it has the information it needs
int totalSectorsRead = 0;
int numberOfSendDataCalls = 0;
uint32_t totalTimeOfSendData_ms = 0;
void send_data(uint32_t sectorCount) {
    numberOfSendDataCalls++;
    uint64_t sectorFront = sectorToSendRegisters[0];
    uint64_t sector = (sectorFront << 32) | sectorToSendRegisters[1];
    printf("Count: %u, Sector: %llu\n", sectorCount, sector);
    int loopCount = 0;
    uint32_t startTime = time_us_32();
    do {
        loopCount++;

        DRESULT dr = disk_read(0, diskReadBuffer, (uint64_t)sector, 1);
        totalSectorsRead++;
        
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
    uint32_t totalTime = time_us_32() - startTime;
    totalTimeOfSendData_ms += totalTime / 1000;
    
    #if PRINT_BUFFER_AFTER_SEND == 1
    // if (sector == 31838 || sector == 41350 || sector == 41351) {
        printf("buffer for sector: %ld\n", sector);
        for (uint diskBufferIndex = 0; diskBufferIndex < DISK_READ_BUFFER_SIZE; diskBufferIndex++) {
            if (diskBufferIndex % 16 == 0) {
                printf("\n%08x: ", diskBufferIndex);
            }
            printf("%02x ", diskReadBuffer[diskBufferIndex]);
        }
        printf("\n");
    // }
    #endif
}

void send_sd_card_data() {
    // Reset send data flag
    sendDataReady = false; 

    // Send the data over uart back to MCU1 so the rom can read it
    // Sector is fetched from the sectorToSendRegisters
    // uint32_t numSectors = 1;
    send_data(1);
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

void testFunction() {
    
    char* filename = "GoldenEye 007 (U) [!].z64";
    char buf[64];
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
    int currentPSRAMChip = 3;
	do {
        fr = f_read(&fil, buf, sizeof(buf), &len);
        //program_write_buf(total, buf, len);
		total += len;
        printf("len: %d\n", len);

	} while (total < 512);
	uint64_t t1 = to_us_since_boot(get_absolute_time());
	uint32_t delta = (t1 - t0) / 1000;
	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

	printf("Read %d bytes in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

	fr = f_close(&fil);
	if (FR_OK != fr) {
		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	printf("---- read file done -----\n\n\n");
}

// If we want to use any flash chips this will erase
// Do this before writing any data to the flash chips
// If any of the chips are flash, erase sectors in advance
void prepare_memory_array_for_rom_data(uint startChipIndex, uint64_t filesize) {

    // uint numPSRAMChips = 0;
    // uint numFlashChips = 0;
    // uint64_t fs = filesize;
    // uint i = 0;
    // do {
    //     uint8_t isFlashChip = isChipIndexFlash(i + startChipIndex);
    //     uint32_t chipSize;
    //     if (isFlashChip) {
    //         numFlashChips++;
    //         chipSize = FLASH_CHIP_CAPACITY_BYTES;
    //     } else {
    //         numPSRAMChips++;
    //         chipSize = PSRAM_CHIP_CAPACITY_BYTES;
    //     }

    //     // Avoid negative numbers, this means we reached the end
    //     if (chipSize > fs) {
    //         fs = 0;
    //     } else {
    //         fs -= chipSize;
    //     }
    //     i++;
    // } while(fs > 0);
    
    uint endChipIndex = (filesize / 1024 / 1024);
    uint index = 0;
    uint64_t remainingFilesize = filesize;
    do {
        psram_set_cs(index + startChipIndex);

        // If this is a flash chip we have to erase it (or a portion of it)
        if (isChipIndexFlash(index + startChipIndex)) {
            uint32_t address = 0;
            uint32_t bytesRemaining;
            if (FLASH_CHIP_CAPACITY_BYTES > remainingFilesize) {
                bytesRemaining = remainingFilesize;
            } else {
                bytesRemaining = FLASH_CHIP_CAPACITY_BYTES;
            }

            printf("Erasing %uMB of flash chip U%u\n", bytesRemaining / 1024 / 1024, index + startChipIndex);
            uint32_t start = time_us_32();
            do {
                program_flash_range_erase(address, FLASH_BLOCK_SIZE, FLASH_BLOCK_SIZE, 0xd8);
                address += FLASH_BLOCK_SIZE;
            } while (address < bytesRemaining);
            uint32_t timeTook = time_us_32() - start;
            uint32_t KB_per_sec = (bytesRemaining / 1024) / (timeTook / 1000000);
            printf("Erased %uMB in %usec [%ukB/s]\n", bytesRemaining / 1024 / 1024, timeTook / 1000000, KB_per_sec);
            remainingFilesize -= bytesRemaining;

        } else {
            printf("PSRAM chip at U%u, no need to erase\n", index + startChipIndex);
            remainingFilesize -= PSRAM_CHIP_CAPACITY_BYTES;
        }

        index++;
    } while(remainingFilesize > 0);

    psram_set_cs(startChipIndex);
}
