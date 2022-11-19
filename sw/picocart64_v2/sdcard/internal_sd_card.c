/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/structs/systick.h"
#include "hardware/flash.h"

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

#define RUN_QSPI_PERMUTATION_TESTS 0
#define ROM_WRITE_OFFSET 1
#define FLASH_TARGET_OFFSET 256 * 1024
char buf[1024 / 2 / 2 / 2 / 2];
// uint8_t buf[FLASH_PAGE_SIZE];
// uint8_t flash_buf[FLASH_PAGE_SIZE];

void __no_inline_not_in_flash_func(load_rom2)(const char* filename) {
    // Renable normal flash stuff?
    // printf("--------------------\n");
    // qspi_print_pull();
    // qspi_oeover_normal(true);
    // printf("--------------------\n");
    // qspi_print_pull();
    // printf("--------------------\n");
    // dump_current_ssi_config();
    // qspi_restore_to_startup_config();
	// ssi_hw->ssienr = 1;

    // qspi_init_spi();
    qspi_enable();
    // qspi_enter_cmd_xip();

    // qspi_oeover_normal(true); // disable CS

    // hw_write_masked(&ioqspi_hw->io[1].ctrl,
    //                 GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
    //                 IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

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
    
    
    printf("DONE!\nLoading rom into flash\n");

    uint8_t txbuf[64] = { 0x05 };
    uint8_t rxbuf[64];
    flash_do_cmd(txbuf, rxbuf, 1);

    printf("register status %x\n", rxbuf[0]);
    qspi_print_pull();

    // printf("Erasing flash...\n");
    // psram_set_cs(1);
    // flash_range_erase(total, FLASH_PAGE_SIZE*385);
    // psram_set_cs(0);
    // printf("DONE!\nProgramming flash...\n");

	uint64_t t0 = to_us_since_boot(get_absolute_time());
	do {        
        fr = f_read(&fil, buf, sizeof(buf), &len);
        psram_set_cs(1);
        // flash_range_program(total, buf, len);
        qspi_write(total, buf, len);
        psram_set_cs(0);
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

    volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    uint32_t cycleCountStart = 0;
    int totalReadTime = 0;
    for (int i = 0; i < 128; i++) {
        uint32_t modifiedAddress = i;
        
        uint32_t startTime_us = time_us_32();
        psram_set_cs(1);
        uint32_t word = ptr[modifiedAddress];
        psram_set_cs(0);

        totalReadTime += time_us_32() - startTime_us;
        if (i < 16) { // only print the first 16 words
            printf("FLASH-MCU2[%d]: %08x\n",i, word);
        }
    }

    printf("\n128 32bit reads @ 0x10000000 reads took %d us\n", totalReadTime);

    uint32_t rom_buf[128];
    uint32_t startTime_us = time_us_32();
    psram_set_cs(1);
    flash_bulk_read(0, rom_buf, 0, sizeof(rom_buf), 0);
    psram_set_cs(0);

    uint32_t dma_totalTime = time_us_32() - startTime_us;
    for(int i = 0; i < 16; i++) {
        printf("DMA[%d]: %08x\n",i, rom_buf[i]);
    }
    printf("DMA read %d 32bit value reads in %d us\n", 128, dma_totalTime);
}

void __no_inline_not_in_flash_func(load_rom)(const char *filename)
{
	// Set output enable (OE) to normal mode on all QSPI IO pins except SS
	qspi_enable();

    // printf("MCU2 QSPI_ENABLED... DUMPING CONFIG\n");
    // dump_current_ssi_config();

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

	//qspi_enter_cmd_xip();

    systick_hw->csr = 0x5;
    systick_hw->rvr = 0x00FFFFFF;

    qspi_enable();
    printf("Read from PSRAM via qspi_read1\n");
    char buf2[4096];
    uint32_t totalTime = 0;
    uint32_t startTimeReadFunction = time_us_32();
    qspi_read(0, buf2, 4096);
    totalTime = time_us_32() - startTimeReadFunction;
    printf("00000000: ");
    for(int i = 0; i < 16; i++) {
        if (i % 8 == 0 && i != 0) {
            printf("\n%08x: ", i);
        }
        printf("%02x ", buf2[i]);
    }
    printf("\n128 32bit (4096 bytes) reads with qspi_read took %dus\n", totalTime);

    // QSPI(actual quad spi) READS DON'T WORK
    // Try to do qspi reads
    qspi_enter_cmd_xip();
    qspi_init_qspi();
    // printf("MCU2 QSPI_XIP ENABLED... DUMPING CONFIG\n");
    // dump_current_ssi_config();

    printf("\n\nRead with XIP in QSPI(real quad) mode\n");
    volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
    for (int i = 0; i < 16; i++) {
        uint32_t modifiedAddress = i;
        psram_set_cs(2);
        uint32_t word = ptr[modifiedAddress];
        psram_set_cs(0);
        printf("PSRAM-MCU2[%d]: %08x\n",i, swap16(word));
    }

    printf("\nFAST READ  - DMA TRANSFER\n");
    // qspi_enter_cmd_xip();
    uint32_t dmaBuffer[128];
    uint32_t startTime_us = time_us_32();
    psram_set_cs(2);
    flash_bulk_read(0x0B, dmaBuffer, 0, 128, 0);
    psram_set_cs(0);
    uint32_t dma_totalTime = time_us_32() - startTime_us;
    for(int i = 0; i < 16; i++) {
        printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    }
    printf("FAST READ - DMA read 128 32bit reads in %d us\n", dma_totalTime);

    // Now see if regular reads work
    qspi_enter_cmd_xip();
    // printf("MCU2 XIP ENABLED... DUMPING CONFIG\n");
    // dump_current_ssi_config();
    printf("\n\nWITH qspi_enter_cmd_xip\n");
    ptr = (volatile uint32_t *)0x10000000;
    uint32_t cycleCountStart = 0;//systick_hw->cvr
    int psram_csToggleTime = 0;
    int total_memoryAccessTime = 0;
    int totalReadTime = 0;
    for (int i = 0; i < 128; i++) {
        uint32_t modifiedAddress = i;
        
        uint32_t startTime_us = time_us_32();
        // uint32_t n = systick_hw->cvr;
        psram_set_cs(2);
        // psram_csToggleTime += systick_hw->cvr - n;
        
        // uint32_t startTime_ticks = systick_hw->cvr;
        uint32_t word = ptr[modifiedAddress];
        // total_memoryAccessTime += systick_hw->cvr - startTime_ticks;
        
        // n = systick_hw->cvr;
        psram_set_cs(0);
        // psram_csToggleTime += systick_hw->cvr - n;

        totalReadTime += time_us_32() - startTime_us;
        if (i < 16) { // only print the first 16 words
            printf("PSRAM-MCU2[%d]: %08x\n",i, word);
        }
    }

    printf("\n128 32bit reads @ 0x10000000 reads took %d us\n", totalReadTime);
    totalTime = 0;

    // This test doesn't quite work right, the data isn't all right
    // volatile uint16_t *ptr_16 = (volatile uint16_t *)0x10000000;
    // for (int i = 0; i < 256;) {
    //     uint32_t startTime_us = time_us_32();
    //     psram_set_cs(2));
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
                                // psram_set_cs(2));
                                // flash_bulk_read(cmd, p_buffer, 0, 128, 0);
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
                                    psram_set_cs(2);
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
    printf("\nDMA TRANSFER\n");
    qspi_enter_cmd_xip();
    // uint32_t dmaBuffer[128];
    startTime_us = time_us_32();
    psram_set_cs(2);
    flash_bulk_read(0x03, dmaBuffer, 0, 128, 0);
    psram_set_cs(0);
    dma_totalTime = time_us_32() - startTime_us;
    for(int i = 0; i < 16; i++) {
        printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    }
    printf("DMA read 128 32bit reads in %d us\n", dma_totalTime);

    // startTime_us = time_us_32();
    // psram_set_cs(1);
    // flash_bulk_read(0x03, dmaBuffer, 0, 1, 0);
    // psram_set_cs(0);
    // dma_totalTime = time_us_32() - startTime_us;
    // for(int i = 0; i < 16; i++) {
    //     printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    // }
    // printf("DMA read 1 32bit value in %d us\n", dma_totalTime);

    // uint32_t oneWordReadStartTime = time_us_32();
    // psram_set_cs(1);
    // uint32_t word = ptr[0];
    // psram_set_cs(0);
    // uint32_t oneWordReadTotalTime = time_us_32() - oneWordReadStartTime;
    // printf("PTR read 1 32bit value in %d us\n", oneWordReadTotalTime);

    // oneWordReadStartTime = time_us_32();
    // word = dmaBuffer[0];
    // oneWordReadTotalTime = time_us_32() - oneWordReadStartTime;
    // printf("1 32bit read from an array in %d us\n", oneWordReadTotalTime);


    // QSPI fast read and fast read quad just don't work :(
    // printf("Init fast read qspi and then DMA\n");
    // qspi_init_qspi();
    // startTime_us = time_us_32();
    // psram_set_cs(1);
    // flash_bulk_read(0x0b, dmaBuffer, 0, 128, 0);
    // psram_set_cs(0);
    // dma_totalTime = time_us_32() - startTime_us;
    // for(int i = 0; i < 16; i++) {
    //     printf("DMA[%d]: %08x\n",i, dmaBuffer[i]);
    // }
    // printf("DMA(fast read) read 128 32bit value in %d us\n", dma_totalTime);

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
