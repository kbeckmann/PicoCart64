/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"

#include "FreeRTOS.h"
#include "task.h"

#include "esp32_task.h"
#include "n64_cic.h"
#include "git_info.h"
#include "led_task.h"
#include "reset_reason.h"
#include "pins_mcu2.h"
#include "utils.h"
#include "qspi_helper.h"
#include "gpio_helper.h"

#include "sdcard/internal_sd_card.h"
#include "pio_uart/pio_uart.h"
#include "psram.h"
#include "flash_array/flash_array.h"
#include "flash_array/program_flash_array.h"

#include "ff.h"
#include <string.h>

#define UART0_BAUD_RATE  (115200)

// Priority 0 = lowest, 31 = highest
// Use same priority to force round-robin scheduling
#define LED_TASK_PRIORITY     (tskIDLE_PRIORITY + 1UL)
#define ESP32_TASK_PRIORITY   (tskIDLE_PRIORITY + 1UL)
#define MAIN_TASK_PRIORITY    (tskIDLE_PRIORITY + 1UL)

static StaticTask_t main_task;
static StaticTask_t led_task;
static StaticTask_t esp32_task;

#define MAIN_TASK_STACK_SIZE (1024)
#define LED_TASK_STACK_SIZE (1024)
#define ESP32_TASK_STACK_SIZE (1024)

static StackType_t main_task_stack[MAIN_TASK_STACK_SIZE];
static StackType_t led_task_stack[LED_TASK_STACK_SIZE];
static StackType_t esp32_task_stack[ESP32_TASK_STACK_SIZE];

static const gpio_config_t mcu2_gpio_config[] = {
	{PIN_UART0_TX, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_UART},
	{PIN_UART0_RX, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_UART},

	// TODO: Configure as PIO0 probably?
	{PIN_SD_CLK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_CMD, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT0_UART1_TX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT1_UART1_RX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_SD_DAT3, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// TODO: Configure as PIO1 probably?
	{PIN_ESP32_D0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_D3, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_CS, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_ESP32_SCK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Demux should be configured as inputs without pulls until we lock the bus
	{PIN_DEMUX_A0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_IE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// MCU1 RUN/RESETn pin
	{PIN_MCU1_RUN, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// WS2812 RGB LED
	{PIN_LED, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Configure as a Clock Output clk_gpout0 after GPIO config
	{PIN_MCU2_GPIO21, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_2MA, GPIO_FUNC_SIO},

	// N64 signals
	{PIN_N64_COLD_RESET, GPIO_IN, false, false, true, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled down
	{PIN_N64_NMI, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up, open drain
	{PIN_CIC_DIO, GPIO_IN, false, true, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},	// Pulled up
	{PIN_CIC_DCLK, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},

	// Configure as PIO that implements UART becase of the way the pins from MCU1 are connected to MCU2
	{PIN_SPI1_SCK, GPIO_IN, true, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1}, 
	//{PIN_SPI1_TX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1}, // not using
	{PIN_SPI1_RX, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
	{PIN_SPI1_CS, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_PIO1},
};

#define NEED_LOAD_ROM 1
void main_task_entry(__unused void *params)
{
	int count = 0;
	printf("MCU2 Main Entry\n");

	// current_mcu_enable_demux(true);
	// psram_set_cs(2);

	// program_connect_internal_flash();
	// program_flash_exit_xip();
	// program_flash_flush_cache();
	// picocart_boot2_enable();

	// volatile uint16_t *ptr16 = (volatile uint16_t *)0x10000000;
	// printf("Access using 16bit pointer at [0x10000000]\n");
	// uint32_t startTime = time_us_32();
	// uint32_t totalTime = 0;
	// for(int i = 0; i < 4096; i+=2) {
	// 	// uint32_t now = time_us_32();	
	// 	volatile uint16_t word = ptr16[i >> 1];

	// 	// totalTime += time_us_32() - now;
	// 	// if (i < 64) {
	// 	// 	if (i % 8 == 0) {
	// 	// 		printf("\n%08x: ", i);
				
	// 	// 	}
	// 	// 	printf("%04x ", word);
	// 	// }
	// }
	// totalTime = time_us_32() - startTime;
	// float elapsed_time_s = 1e-6f * totalTime;
	// printf("\nxip access via boot2 for 4kB using 16bit pointer took %d us. %.3f MB/s\n", totalTime, ((4096 / 1e6f) / elapsed_time_s));

	// // boot mcu1 before loading rom so it can actually read out of flash to boot
	// if (NEED_LOAD_ROM == 0) {
	// 	printf("Loading rom...\n");
	// 	load_rom("Doom 64 (USA) (Rev 1).z64");
	// 	// load_rom("testrom.z64"); 
	// 	printf("\nfinished loading rom.\n");
	// } else {
	// 	printf("Skipping rom load this time. Rom should already be in flash.\n");
	// }

	// psram_set_cs(1);
	// current_mcu_enable_demux(false);
	
	// ssi_hw->ssienr = 0;
	// qspi_oeover_disable();

	vTaskDelay(100);
	printf("Booting MCU1...\n");
	gpio_put(PIN_MCU1_RUN, 1);

	printf("Mounting SD Card...");
	mount_sd();
	printf("Finished!\n");

	// Setup PIO UART
	printf("Initing MCU1<->MCU2 serial bridge...");
	pio_uart_init(PIN_SPI1_CS, PIN_SPI1_RX);
	printf("Finshed!\n");
	
	//pc64_load_new_rom_command("Doom 64 (USA) (Rev 1).z64");
	// load_rom("Doom 64 (USA) (Rev 1).z64");

	volatile uint32_t t = 0;
	while (true) {
		tight_loop_contents();

		if(time_us_32() - t > 1000000) {
			// printf(". ");
			// test_read_from_psram();
			t = time_us_32();

			// if (count == 0) {
			// 	count++;
			// 	printf("Booting MCU1...\n");
			// 	gpio_put(PIN_MCU1_RUN, 1);
			// }
			// count++;
			// if (count % 60 == 0 && count != 0) {
			// 	printf("Resetting core 1\n");
			// 	gpio_put(PIN_MCU1_RUN, 0);
			// 	for(int k = 0; k < 10000; k++) {
			// 		tight_loop_contents();
			// 	}
			// 	gpio_put(PIN_MCU1_RUN, 1);
			// }
		}

		// process the buffer look for cmd data
		mcu2_process_rx_buffer();

		if(sendDataReady) {
			send_sd_card_data();
		}

		if (startRomLoad && !romLoading) {
			romLoading = true;
			printf("Loading rom!\n");
			load_new_rom("Doom 64 (USA) (Rev 1).z64");
			romLoading = false;
			startRomLoad = false;
		}

#if 0
		printf("----------------------------------------\n");
		printf("MCU 2 [i=%d]\n", count);
		printf("Stack usage main_task: %d bytes\n", MAIN_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark(NULL));
		printf("Stack usage led_task: %d bytes\n", LED_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark((TaskHandle_t) & led_task));
		printf("Stack usage esp32_task: %d bytes\n", ESP32_TASK_STACK_SIZE - uxTaskGetStackHighWaterMark((TaskHandle_t) & esp32_task));
		printf("----------------------------------------\n\n");
#endif
	}
}

void mcu2_core1_entry(void)
{
	printf("[Core1] CIC Starting\n");

	while (1) {
		n64_cic_run(PIN_N64_COLD_RESET, PIN_CIC_DCLK, PIN_CIC_DIO);

		// n64_cic_run returns when N64_CR goes low, i.e.
		// user presses the reset button, or the N64 loses power.

		// TODO: Perform actions when this happens. Commit RAM to flash/SD etc.

		printf("[Core1] CIC Restarting\n");
	}
}

void vLaunch(void)
{
	xTaskCreateStatic(main_task_entry, "Main", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, main_task_stack, &main_task);
	xTaskCreateStatic(led_task_entry, "LED", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, led_task_stack, &led_task);
	//xTaskCreateStatic(esp32_task_entry, "ESP32", ESP32_TASK_STACK_SIZE, NULL, ESP32_TASK_PRIORITY, esp32_task_stack, &esp32_task);

	// Start the tasks and timer.
	vTaskStartScheduler();
}

void mcu2_main(void)
{
	const int freq_khz = 133000;
	// const int freq_khz = 166000;
	// const int freq_khz = 200000;
	// const int freq_khz = 210000;
	// const int freq_khz = 220000;
	// const int freq_khz = 230000;
	// const int freq_khz = 240000;
	// const int freq_khz = 266000;

	// Note that this might call set_sys_clock_pll,
	// which might set clk_peri to 48 MHz
	bool clockWasSet = set_sys_clock_khz(freq_khz, false);

	// Init async UART on pin 0/1
	// stdio_async_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	stdio_uart_init_full(DEBUG_UART, DEBUG_UART_BAUD_RATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
	gpio_configure(mcu2_gpio_config, ARRAY_SIZE(mcu2_gpio_config));

	set_demux_mcu_variables(PIN_DEMUX_A0, PIN_DEMUX_A1, PIN_DEMUX_A2, PIN_DEMUX_IE);

	printf("MCU2: Was%s able to set clock to %d MHz\n", clockWasSet ? "" : " not", freq_khz/1000);

	// Enable a 12MHz clock output on GPIO21 / clk_gpout0
	clock_gpio_init(PIN_MCU2_GPIO21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);

	printf("\n\n----------------------------------------\n");
	printf("PicoCart64 MCU2 Boot (git rev %08x)\r\n", GIT_REV);
	printf("Reset reason: 0x%08lX\n", get_reset_reason());
	printf("clk_sys: %d Hz\n", clock_get_hz(clk_sys));
	printf("clk_peri: %d Hz\n", clock_get_hz(clk_peri));
	printf("----------------------------------------\n\n");

	// printf("\n\n");
	// uint32_t timeBuffer[32];
	// systick_hw->csr = 0x5;
    // systick_hw->rvr = 0x00FFFFFF;
	// for(int i = 0; i < 32; i++) {
	// 	uint32_t startTime_ns = systick_hw->cvr;
	// 	timeBuffer[i] = startTime_ns;
	// }

	// for(int i = 0; i < 32; i++) {
	// 	printf("%d\n", timeBuffer[i]);	
	// }

	// printf("\n\n");

	multicore_launch_core1(mcu2_core1_entry);

	// Start FreeRTOS on Core0
	vLaunch();

	while (true) {
		// Never reached
	}
}
