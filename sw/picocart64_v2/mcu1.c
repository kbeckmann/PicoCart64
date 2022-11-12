/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pins_mcu1.h"
#include "qspi_helper.h"
#include "n64_pi_task.h"
#include "stdio_async_uart.h"
#include "reset_reason.h"
#include "sha256.h"
#include "rom_vars.h"

#include "gpio_helper.h"
#include "utils.h"

#include "sdcard/internal_sd_card.h"
#include "pio_uart/pio_uart.h"

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
	{PIN_MCU2_SCK, GPIO_IN, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
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

	// printf("qspi_set_cs(%d)\n", chip);

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

static inline uint32_t read_from_psram2(uint32_t address) {
	volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
	uint32_t modifiedAddress = address / 2;
	psram_set_cs2(psram_addr_to_chip2(modifiedAddress));
	uint32_t word = ptr[modifiedAddress];
	psram_set_cs2(0);
	return address % 2 == 0 ? word : swap16(word);
}

void __no_inline_not_in_flash_func(mcu1_core1_entry)() {
	// pio_uart_init(PIN_MCU2_DIO, PIN_MCU2_CS);

	printf("MCU1 core1 booted!\n");
	
	bool readingData = false;
	uint32_t t = 0;
	uint32_t it = 0;
	while (1) {
		tight_loop_contents();
		
		if(time_us_32() - t > 5000000) {
			t = time_us_32();
			it++;

			if (it == 10) {
				qspi_enable();
				qspi_enter_cmd_xip();

				volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
				printf("Access at [0x10000000]\n");
				for(int i = 0; i < 4; i++) {
					uint32_t address_32 = i;
					psram_set_cs2(1);
					uint32_t word = ptr[address_32];
					psram_set_cs2(0);

					printf("PSRAM-MCU1[%d] = %08x\n", address_32, word);
				}

				qspi_disable();
			} 
			// else if (it > 10) {
			// 	volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
			// 	for(int i = 0; i < 4; i++) {
			// 		uint32_t address_32 = i;
			// 		psram_set_cs2(1);
			// 		uint32_t word = ptr[address_32];
			// 		psram_set_cs2(0);

			// 		printf("PSRAM[%d] = %08x\n", address_32, word);
			// 	}
			// }
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
					
					break;
				default:
					break;
			}
		}
	}
}

void __no_inline_not_in_flash_func(mcu1_main)(void)
{
	int count = 0;
	const int freq_khz = 133000;
	// const int freq_khz = 200000;
	// const int freq_khz = 210000;
	// const int freq_khz = 220000;
	// const int freq_khz = 230000;
	///// below no workie
	// const int freq_khz = 240000;
	// const int freq_khz = 266000;

	set_sys_clock_khz(freq_khz, true);

	gpio_configure(mcu1_gpio_config, ARRAY_SIZE(mcu1_gpio_config));

	// Enable STDIO over USB
	// stdio_usb_init();
	// stdio_async_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	stdio_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	
	// uint32_t BUFFER_SIZE = 6;
	// uint8_t writeBuffer[BUFFER_SIZE];
	// uint8_t readBuffer[BUFFER_SIZE];
	// //Test to send some characters using pio
	// writeBuffer[0] = 'H';
	// writeBuffer[1] = 'E';
	// writeBuffer[2] = 'L';
	// writeBuffer[3] = 'L';
	// writeBuffer[4] = 'O';
	// writeBuffer[5] = '\n';
	// int lastWrite = 0;
	// while(1) {
	// 	int now = time_us_32();
	// 	if (now - lastWrite > 6000000) {
	// 		for (int i = 0; i < BUFFER_SIZE; i++) {
	// 			uart_tx_program_putc(writeBuffer[i]);
	// 		}
	// 		lastWrite = now;
	// 	}
	// }

#if 0
	int pin = PIN_N64_AD0;

	printf("Testing pin %d\n", pin);

	gpio_set_dir(pin, GPIO_OUT);
	gpio_put(pin, 1);

	while (1) {

	}
#endif

	// gpio_set_pulls(PIN_N64_COLD_RESET, false, true);
	// gpio_set_pulls(PIN_N64_SI_DAT, true, false);
	// gpio_set_pulls(PIN_N64_INT1, true, false);

	//printf("Hello, world! I am MCU 1 @%d kHz. Reset reason: 0x%08lX\n", freq_khz, get_reset_reason());

	//sleep_ms(100);

#if 0
	while (true) {
		count++;
		printf("Hello, world! I am MCU 1 -- %d\n", count);
		sleep_ms(1000);
	}

#else

	// This can't be enabled when mcu2 is accessing psram
	// qspi_oeover_normal(true);
	// ssi_hw->ssienr = 1;

	// sleep_ms(10);

#if 0
	const uint32_t *data = (const uint32_t *)0x10000000;
	for (int i = 0; i < 1024 * 1024; i++) {
		printf("%08x ", *data++);
		printf("%08x ", *data++);
		printf("%08x ", *data++);
		printf("%08x\n", *data++);
		sleep_ms(10);
	}
#endif

	// qspi_print_pull();

#if 0
	while (true) {
		char hash_str[128];

		sha256_to_string(hash_str, (const BYTE *)0x10000000, 30228);
		printf("Hash: %s\n", hash_str);

		sleep_ms(500);
	}

#elif 0

	int unique = 0;
	int i = 0;
	while (true) {
		char hash_str[65];
		char hash_str_old[65];

		sha256_to_string(hash_str, (const BYTE *)0x10000000, 1024 * 1024);
		if (strcmp(hash_str, hash_str_old) != 0) {
			strcpy(hash_str_old, hash_str);
			unique++;
			printf("Hash: %s, unique: %d\n", hash_str, unique);
		}
		// if (i % 100 == 0) {
		printf("Still alive, %d %d\n", i, unique);
		// }

		i++;
		// sleep_ms(500);
	}

#else

	{
		char hash_str[128];
		sha256_to_string(hash_str, (const BYTE *)0x10000000, 1079028);
		printf("Hash: %s\n", hash_str);
	}

#endif

	// // Set up ROM mapping table
	// if (memcmp(picocart_header, "picocartcompress", 16) == 0) {
	// 	// Copy rom compressed map from flash into RAM
	// 	// uart_tx_program_puts("Found a compressed ROM\n");
	// 	memcpy(rom_mapping, flash_rom_mapping, MAPPING_TABLE_LEN * sizeof(uint16_t));
	// } else {
	// 	for (int i = 0; i < MAPPING_TABLE_LEN; i++) {
	// 		rom_mapping[i] = i;
	// 	}
	// }

	// Put something in this array for sanity testing
	for(int i = 0; i < 256; i++) {
		pc64_uart_tx_buf[i] = 0xFFFF - i;
	}

	multicore_launch_core1(mcu1_core1_entry);

	// printf("launching n64_pi_run...\n");
	// n64_pi_run();

#endif

	while (true) {

	}

}
