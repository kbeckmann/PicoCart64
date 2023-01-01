/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/systick.h"
#include "pico/time.h"

#include "pins_mcu1.h"
#include "n64_pi_task.h"
#include "reset_reason.h"
#include "sha256.h"

#include "stdio_async_uart.h"

#include "gpio_helper.h"
#include "utils.h"

#include "qspi_helper.h"
#include "sdcard/internal_sd_card.h"
#include "pio_uart/pio_uart.h"
#include "psram.h"
#include "flash_array/flash_array.h"
#include "flash_array/program_flash_array.h"

#include "rom_vars.h"

static const gpio_config_t mcu1_gpio_config[] = {
	// PIO0 pins
	{PIN_N64_AD0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD3, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD4, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD5, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD6, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD7, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD8, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD9, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD10, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD11, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD12, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD13, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD14, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_AD15, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_ALEL, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_ALEH, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_WRITE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},
	{PIN_N64_READ, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO0},

	// Remaining N64 pins are treated as normal GPIOs
	{PIN_N64_COLD_RESET, GPIO_IN, false, false, true, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled down
	{PIN_N64_SI_DAT, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up, open drain
	{PIN_N64_INT1, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up, open drain

	// Demux should be configured as inputs without pulls until we lock the bus
	{PIN_DEMUX_A0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_IE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// SPI on PIO1
    // PIN_MCU2_SCK        (27) // MCU2 GPIO pin 26: PIN_SPI1_SCK
    // PIN_MCU2_CS         (28) // MCU2 GPIO pin 29: PIN_SPI1_CS
    // PIN_MCU2_DIO        (29) // MCU2 GPIO pin 28: PIN_SPI1_RX
	// {PIN_MCU2_SCK, GPIO_IN, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	// {PIN_MCU2_SCK, GPIO_OUT, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1}, // used when spi master
	{PIN_MCU2_CS, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
	// {PIN_MCU2_CS, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_UART},	// UART for now
	{PIN_MCU2_DIO, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
};

static inline uint8_t psram_addr_to_chip2(uint32_t address)
{
	return ((address >> 23) & 0x7) + 1;
}

//   0: Deassert all CS
// 1-8: Assert the specific PSRAM CS (1 indexed, matches U1, U2 ... U8)
static inline void psram_set_cs2(uint8_t chip)
{
	uint32_t mask = (1 << PIN_DEMUX_IE) | (1 << PIN_DEMUX_A0) | (1 << PIN_DEMUX_A1) | (1 << PIN_DEMUX_A2);
	uint32_t new_mask;

	if (chip >= 1 && chip <= 8) {
		chip--;					// convert to 0-indexed
		new_mask = (1 << PIN_DEMUX_IE) | (chip << PIN_DEMUX_A0);
	} else {
		// Set PIN_DEMUX_IE = 0 to pull all PSRAM CS-lines high
		new_mask = 0;
	}

	uint32_t old_gpio_out = sio_hw->gpio_out;
	sio_hw->gpio_out = (old_gpio_out & (~mask)) | new_mask;
}

volatile uint32_t *rom_ptr = (volatile uint32_t *)0x10000000;
static inline uint32_t read_from_psram2(uint32_t address) {
	uint32_t word = rom_ptr[address];
	return swap16(word);
}

uint32_t log_buffer[LOG_BUFFER_SIZE]; // store addresses
volatile int log_head = 0;
volatile int log_tail = 0;

volatile uint32_t lastLoggedValue = 0;
volatile int compactedSequensialValues = 0;
void add_log_to_buffer(uint32_t value) {
	log_buffer[log_head++] = value;
	if (log_head >= LOG_BUFFER_SIZE) {
		log_head = 0;
	}
}

static uint32_t last_log_value = 0;
void process_log_buffer() {
	if (log_tail == log_head) {
		// noting to pring
		return;
	}

	uint32_t value = log_buffer[log_tail++];

	// if (lastLoggedValue+2 == value) {
	// 	compactedSequensialValues++;
	// } else if (value > lastLoggedValue) {
	// 	printf("!%08x ", lastLoggedValue);
	// 	if (compactedSequensialValues > 0) {
	// 		printf("!%d ||", compactedSequensialValues);
	// 	}
	// 	compactedSequensialValues = 0;
	// }
	// lastLoggedValue = value;

	// if (log_tail % 64 == 0) {
	// 	printf("\n");
	// }

	printf("0x%08x ", value);
	// printf("%d ", value - last_log_value);
	// last_log_value = value;
	if (log_tail >= LOG_BUFFER_SIZE) {
		log_tail = 0;
		// printf("\n");
	}
}

uint32_t last_rom_cache_update_address = 0;
void __no_inline_not_in_flash_func(mcu1_core1_entry)() {
	pio_uart_init(PIN_MCU2_DIO, PIN_MCU2_CS);

	printf("MCU1 core1 booted!\n");
	uart_tx_program_putc(0xA);
	uart_tx_program_putc(0xB);
	uart_tx_program_putc(0xC);
	
	bool readingData = false;
	volatile bool hasInit = false;
	volatile bool waitingForRomLoad = false;
	volatile uint32_t t = 0;
	volatile uint32_t it = 0;
	while (1) {
		tight_loop_contents();
		
		// if(time_us_32() - t > 1000000) {
		// 	t = time_us_32();
		// 	it++;

		// 	if (it > 3 && !hasInit) {
		// 		hasInit = true;

		// 		printf("\nMCU1 try to read with ptr\n");

		// 		// THIS IS FOR FLASH READING
		// 		// current_mcu_enable_demux(true);
    	// 		// psram_set_cs(2);
		// 		// program_connect_internal_flash();
		// 		// program_flash_exit_xip();
		// 		// program_flash_flush_cache();
		// 		// picocart_boot2_enable();

		// 		uint32_t largestAccessTime = 0;
		// 		uint32_t smallestAccessTime = 100000;

		// 		// systick_hw->csr = 0x5;
    	// 		// systick_hw->rvr = 0x00FFFFFF;

		// 		volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;
		// 		printf("Access using 16bit pointer at [0x10000000]\n");
		// 		volatile uint16_t buffer[2048];
		// 		uint32_t totalTime = 0;	
		// 		for(int i = 0; i < 4096; i+=2) {
		// 			uint32_t startTime = time_us_32();	
		// 			volatile uint16_t word = ptr16[i >> 1];
		// 			buffer[i >> 1] = word;
		// 			uint32_t accessTime = time_us_32() - startTime;
		// 			totalTime += accessTime;

		// 			if (largestAccessTime < accessTime) {
		// 				largestAccessTime = accessTime;
		// 			} 

		// 			if (smallestAccessTime > accessTime) {
		// 				smallestAccessTime = accessTime;
		// 			}

		// 			if (i < 64) {
		// 				if (i % 8 == 0) {
		// 					printf("\n%08x: ", i);
							
		// 				}
		// 				printf("%04x ", word);
		// 			}
		// 		}
		// 		// totalTime = time_us_32() - startTime;
		// 		float elapsed_time_s = (1e-6f * totalTime);
		// 		printf("\nxip access for 4kB via 16bit pointer took %d us. %.3f MB/s\n", totalTime, ((4096 / 1e6f) / elapsed_time_s));
		// 		printf("Variance-> Longest Access: %d, Shortest: %d\n", largestAccessTime, smallestAccessTime);

		// 		// systick_hw->csr = 0x5;
    	// 		// systick_hw->rvr = 0x00FFFFFF;

		// 		// printf("\n\nTake 2\n");
		// 		// totalTime = 0;
		// 		// for(int i = 0; i < 4096; i+=2) {
		// 		// 	// now = time_us_32();	
		// 		// 	uint32_t startTime_ns = systick_hw->cvr;
		// 		// 	uint16_t word = ptr16[i >> 1];
		// 		// 	uint32_t endTime_ns = systick_hw->cvr;
		// 		// 	float accessTime = startTime_ns - endTime_ns;//time_us_32() - now;
		// 		// 	totalTime += accessTime;

		// 		// 	if (largestAccessTime < accessTime) {
		// 		// 		largestAccessTime = accessTime;
		// 		// 	} 

		// 		// 	if (smallestAccessTime > accessTime) {
		// 		// 		smallestAccessTime = accessTime;
		// 		// 	}

		// 		// 	if (i < 64) {
		// 		// 		if (i % 8 == 0) {
		// 		// 			printf("\n%08x: ", i);
							
		// 		// 		}
		// 		// 		printf("%04x ", word);
		// 		// 	}
		// 		// }
		// 		// elapsed_time_s = (1e-6f * totalTime) * 5;
		// 		// printf("\nxip(via boot2) access for 4kB via 16bit pointer took %f cycles. %.3f MB/s\n", totalTime, ((4096 / 1e6f) / elapsed_time_s));
		// 		// printf("Variance-> Longest Access: %d, Shortest: %d\n", largestAccessTime, smallestAccessTime);

		// 		//load_rom_cache(0);
		// 	}
		// }

		// process_log_buffer();

		if (waitingForRomLoad) {
			if(time_us_32() - t > 1000000) {
				t = time_us_32();
				it++;
			
				// wait 30 seconds then release
				if (it > 30 && !hasInit) {
					hasInit = true;
					current_mcu_enable_demux(true);
					psram_set_cs2(3); // use the psram chip
					
					// Reads should be enabled now
					g_loadRomFromMemoryArray = true; // read from psram
					waitingForRomLoad = false;
					sd_is_busy = false;
				}
			}
		}

		if (readingData) {
			// Process anything that might be on the uart buffer
			mcu1_process_rx_buffer();

			if (sendDataReady) {
				// Now that the data is written to the array, go ahead and release the lock
				sd_is_busy = false;
				readingData = false;
			}
		}

		if (multicore_fifo_rvalid()) {
			int32_t cmd = multicore_fifo_pop_blocking();
			switch (cmd) {
				case CORE1_SEND_SD_READ_CMD:
					// Block cart while waiting for data
					sd_is_busy = true;

					// Finally start processing the uart buffer
					readingData = true;
					rx_uart_buffer_reset();
					
					pc64_send_sd_read_command();
				case CORE1_UPDATE_ROM_CACHE:
					printf("Not using cache. Uncomment code to load cache...\n");
					// if (last_rom_cache_update_address != update_rom_cache_for_address) {
					// 	update_rom_cache(update_rom_cache_for_address);
					// 	last_rom_cache_update_address = update_rom_cache_for_address;
					// 	printf("Cache update %08x\n", update_rom_cache_for_address);
					// }
					break;
				case CORE1_LOAD_NEW_ROM_CMD:
					waitingForRomLoad = true;
					pc64_send_load_new_rom_command();

				default:
					break;
			}
		}
	}
}

const uint32_t timeout_us = 10000 * 1000; // 10 seconds
int test_line_read(int gpio, char* name) {
	uint32_t start_time = time_us_32();
	// uint32_t current_time = start_time;
	int value = gpio_get(gpio);
	while(value == false) {
		tight_loop_contents();
		if ((time_us_32()-start_time) >= timeout_us) {
			printf("Timeout waiting for %s [gpio %d] to return value\n", name, gpio);
			break;
		}
		// current_time += time_us_32() - start_time;

		value = gpio_get(gpio);
	}

	if (value) {
		printf("%s [GPIO %d] good!\n", name, gpio);
	}

	return value;
}

void boardTest() {

	// Init the pins and set them all to input

	// init the address/data pins
	for(int i = 0; i < 16; i++) {
		gpio_init(i);
		gpio_set_dir(i, false);
	}

	gpio_init(PIN_N64_ALEH);
    gpio_set_dir(PIN_N64_ALEH, false);
    
    gpio_init(PIN_N64_ALEL);
    gpio_set_dir(PIN_N64_ALEL, false);
    
    gpio_init(PIN_N64_READ);
    gpio_set_dir(PIN_N64_READ, false);
    
    gpio_init(PIN_N64_COLD_RESET);
    gpio_set_dir(PIN_N64_COLD_RESET, false);

	// Wait until the cold reset is true
	while(gpio_get(PIN_N64_COLD_RESET) == false) {
		tight_loop_contents();
	}

	uint32_t start_time = time_us_32();
	uint32_t current_time = start_time;
	// Test data line get values
	printf("\n\nTesting data lines\n");
	for(int i = 0; i < 16; i++) {
		int value = gpio_get(i);

		start_time = time_us_32();
		// current_time = start_time;
		while(!value) {
			if ((time_us_32()-start_time) >= timeout_us) {
				printf("Timeout waiting for gpio %d to return value\n", i);
				break;
			}

			value = gpio_get(i);
		}

		if (value) {
			printf("GPIO/AD [%d] good!\n", i);
		}
	}

	printf("\n\nTesting control lines\n");
	int aleh_good = test_line_read(PIN_N64_ALEH, "ALEH");
	int alel_good = test_line_read(PIN_N64_ALEL, "ALEL");
	int read_good = test_line_read(PIN_N64_READ, "READ");

	// Now test writing to each of the data lines
	for(int i = 0; i < 16; i++) {
		gpio_set_dir(i, true);
		gpio_put(i, true);
	}

	sleep_ms(10);
	// Wait for the read line to go high and signal the end of the test
	test_line_read(PIN_N64_READ, "READ");

	printf("board_test finished!\n");

}

void __no_inline_not_in_flash_func(mcu1_main)(void)
{
	int count = 0;
	const int freq_khz = 133000;
	// const int freq_khz = 166000;
	// const int freq_khz = 200000;
	// const int freq_khz = 210000;
	// const int freq_khz = 220000;
	// const int freq_khz = 230000;
	// const int freq_khz = 240000;
	// const int freq_khz = 266000;
	// const int freq_khz = 166000 * 2;

	bool clockWasSet = set_sys_clock_khz(freq_khz, false);

	gpio_configure(mcu1_gpio_config, ARRAY_SIZE(mcu1_gpio_config));
	set_demux_mcu_variables(PIN_DEMUX_A0, PIN_DEMUX_A1, PIN_DEMUX_A2, PIN_DEMUX_IE);

	// Enable STDIO
	// stdio_async_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	// stdio_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);

	printf("\n\nMCU1: Was%s able to set clock to %d MHz\n", clockWasSet ? "" : " not", freq_khz/1000);

	// IF READING FROM FROM FLASH... (works for compressed roms)
	qspi_oeover_normal(true);
	ssi_hw->ssienr = 1;
	// Set up ROM mapping table
	if (memcmp(picocart_header, "picocartcompress", 16) == 0) {
		// Copy rom compressed map from flash into RAM
		// uart_tx_program_puts("Found a compressed ROM\n");
		printf("Found a compressed ROM\n");
		memcpy(rom_mapping, flash_rom_mapping, MAPPING_TABLE_LEN * sizeof(uint16_t));
	} else {
		for (int i = 0; i < MAPPING_TABLE_LEN; i++) {
			rom_mapping[i] = i;
		}
	}

#if 0
	printf("Start board test\n");
	boardTest();
#else
	// Put something in this array for sanity testing
	for(int i = 0; i < 256; i++) {
		pc64_uart_tx_buf[i] = 0xFFFF - i;
	}

	multicore_launch_core1(mcu1_core1_entry);

	printf("launching n64_pi_run...\n");

	// current_mcu_enable_demux(true);
	// psram_set_cs(2);
	// program_connect_internal_flash();
	// program_flash_exit_xip();
	// program_flash_flush_cache();
	// picocart_boot2_enable();

	n64_pi_run();

	while (true) {

	}
#endif

}
