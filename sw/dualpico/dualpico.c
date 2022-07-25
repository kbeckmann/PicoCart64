/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/xip_ctrl.h"

#include "pc64_rand.h"

/*
 * This code demonstrates how to boot two RP2040 devices with a shared flash chip.
 * MCU1       <-> FLASH    <-> MCU2
 * MCU1.GPIO2  -> MCU2.RUN  -> 10k Pull Down
 * MCU2.GPIO2               -> 10k Pull Up
 *
 * MCU1 boots first, copies program from flash to RAM and executes.
 * MCU1 reads GPIO2. If LOW: we are MCU1.
 * MCU1 sets QSPI OutputEnable override to Disabled.
 * MCU1 drives GPIO2 HIGH, letting MCU2 boot.
 * MCU2 boots, copies program from flash to RAM and executes.
 * MCU2 reads GPIO2. If HIGH: we are MCU2.
 * MCU2 sets QSPI OutputEnable override to Disabled.
 * Done.
 *
 */

// Set up the SSI controller for standard SPI mode,i.e. for every byte sent we get one back
static void flash_init_spi(bool is_slave)
{
	// Disable SSI for further config
	ssi_hw->ssienr = 0;
	// Clear sticky errors (clear-on-read)
	(void)ssi_hw->sr;
	(void)ssi_hw->icr;
	// Hopefully-conservative baud rate for boot and programming
	// ssi_hw->baudr = 6;
	// ssi_hw->baudr = 20;
	ssi_hw->baudr = 0x100;
	uint32_t bits = (1 << SSI_CTRLR0_SSTE_LSB) | (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |	// Standard 1-bit SPI serial frames
		(7 << SSI_CTRLR0_DFS_32_LSB) |	// 8 clocks per data frame
		(SSI_CTRLR0_TMOD_VALUE_TX_AND_RX << SSI_CTRLR0_TMOD_LSB);	// TX and RX FIFOs are both used for every byte

	if (is_slave) {
		bits |= (1 << SSI_CTRLR0_SLV_OE_LSB);	// Slave output enable
	}

	ssi_hw->ctrlr0 = bits;
	// Slave _NOT_ selected when transfers in progress
	// ssi_hw->ser = is_slave ? 0 : 1;
	ssi_hw->ser = 1;
	// Re-enable
	ssi_hw->ssienr = 1;
}

// Put bytes from one buffer, and get bytes into another buffer.
// These can be the same buffer.
// If tx is NULL then send zeroes.
// If rx is NULL then all read data will be dropped.
//
// If rx_skip is nonzero, this many bytes will first be consumed from the FIFO,
// before reading a further count bytes into *rx.
// E.g. if you have written a command+address just before calling this function.
void __noinline flash_put_get(const uint8_t * tx, uint8_t * rx, size_t count, size_t rx_skip)
{
	// Make sure there is never more data in flight than the depth of the RX
	// FIFO. Otherwise, when we are interrupted for long periods, hardware
	// will overflow the RX FIFO.
	const uint max_in_flight = 16 - 2;	// account for data internal to SSI
	size_t tx_count = count;
	size_t rx_count = count;
	while (tx_count || rx_skip || rx_count) {
		// NB order of reads, for pessimism rather than optimism
		uint32_t tx_level = ssi_hw->txflr;
		uint32_t rx_level = ssi_hw->rxflr;
		bool did_something = false;	// Expect this to be folded into control flow, not register
		if (tx_count && tx_level + rx_level < max_in_flight) {
			ssi_hw->dr0 = (uint32_t) (tx ? *tx++ : 0);
			--tx_count;
			did_something = true;
		}
		if (rx_level) {
			uint8_t rxbyte = ssi_hw->dr0;
			did_something = true;
			if (rx_skip) {
				--rx_skip;
			} else {
				if (rx)
					*rx++ = rxbyte;
				--rx_count;
			}
		}
		// APB load costs 4 cycles, so only do it on idle loops (our budget is 48 cyc/byte)
		// if (!did_something && __builtin_expect(flash_was_aborted(), 0))
		//     break;
	}
	// flash_cs_force(OUTOVER_HIGH);
}

void __noinline flash_flush_cache()
{
	xip_ctrl_hw->flush = 1;
	// Read blocks until flush completion
	(void)xip_ctrl_hw->flush;
	// Enable the cache
	hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_EN_BITS);
	// flash_cs_force(OUTOVER_NORMAL);
}

// Put the SSI into a mode where XIP accesses translate to standard
// serial 03h read commands. The flash remains in its default serial command
// state, so will still respond to other commands.
// static void flash_enter_cmd_xip(void)
// {
//     ssi_hw->ssienr = 0;
//     ssi_hw->ctrlr0 =
//             (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
//             (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
//             (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
//     ssi_hw->spi_ctrlr0 =
//             (FLASHCMD_READ_DATA << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
//             (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
//             (6u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
//             (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A  // Command and address both in serial format
//                     << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
//     ssi_hw->ssienr = 1;
// }

#define UART_TX_PIN    (0)
#define UART_RX_PIN    (1)
#define MCU2_RUN_PIN   (2)
#define CUSTOM_CSN_PIN (3)

#define LED_PIN        (25)

#define QSPI_SCLK_PIN  (0)
#define QSPI_CSn_PIN   (1)
#define QSPI_SD0_PIN   (2)
#define QSPI_SD1_PIN   (3)
#define QSPI_SD2_PIN   (4)
#define QSPI_SD3_PIN   (5)

#define QSPI_GPIO_INPUT ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_HIGH     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_OUTPUT_HIGH ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_ENABLE    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_HIGH     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_OUTPUT_LOW ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_ENABLE    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_LOW      << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_INPUT_GET(_pin) (ioqspi_hw->io[(_pin)].status & IO_QSPI_GPIO_QSPI_SCLK_STATUS_INFROMPAD_BITS)

int main(void)
{
	int count = 0;

	// On MCU1, GPIO 2 is pulled low externally and connected to MCU2.RUN.
	// On MCU2, GPIO 2 is pulled high externally.
	gpio_init(MCU2_RUN_PIN);
	gpio_set_dir(MCU2_RUN_PIN, GPIO_IN);

	// GPIO 25 / LED is a slow signal to show if we are running.
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	// mcu_id will be 1 for MCU1, and 2 for MCU2.
	int mcu_id = gpio_get(MCU2_RUN_PIN) + 1;

	stdio_init_all();

	// Turn off SSI
	// ssi_hw->ssienr = 0;

	// Disable OutputEnable for SCK and CS
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_CSn_PIN].ctrl = (ioqspi_hw->io[QSPI_CSn_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	// Turn off the XIP cache
	// xip_ctrl_hw->ctrl = 1 << XIP_CTRL_POWER_DOWN_LSB;
	// xip_ctrl_hw->flush = 1;

	if (mcu_id == 1) {
		// I am MCU1

		// GPIO4 is our custom chip select
		gpio_init(CUSTOM_CSN_PIN);
		gpio_set_dir(CUSTOM_CSN_PIN, GPIO_OUT);
		gpio_put(CUSTOM_CSN_PIN, 1);

		// Let MCU2 boot
		gpio_set_dir(MCU2_RUN_PIN, GPIO_OUT);
		gpio_put(MCU2_RUN_PIN, 1);	// Set GPIO2 / MCU2.RUN to HIGH

		// TODO: Wait for MCU2 to tell MCU1 "I have booted"
		sleep_ms(1000);

		flash_init_spi(false);

		// flash_enter_cmd_xip();

		// ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
		//                          (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

		// Send data to MCU2 by accessing random flash addresses
		while (true) {
			count++;
			gpio_put(LED_PIN, count & 1);	// Toggle LED slowly

			while (ssi_hw->rxflr != 0) {
				// Empty the RX fifo (shouldn't be needed)
				uint32_t value = ssi_hw->dr0;
				printf("** Threw away an RX entry: %08lX\n", value);
			}

			ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
												 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

			// CustomCSn = 0
			gpio_put(CUSTOM_CSN_PIN, 0);

			ssi_hw->dr0 = count;
			for (int i = 0; i < 16; i++) {
				ssi_hw->dr0 = 0;
			}

			while (ssi_hw->rxflr < 16) {
				// Wait until we have two entries in the rx fifo
			}

			// CustomCSn = 1
			gpio_put(CUSTOM_CSN_PIN, 1);

			uint32_t value0 = ssi_hw->dr0;
			uint32_t value1 = ssi_hw->dr0;

			ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
												 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

			printf("Read from SSI: %08lX %08lX\n", value0, value1);
			printf("MCU1 rxflr: %d, txflr: %d, xip_ctrl_hw->stat: %d\n", ssi_hw->rxflr, ssi_hw->txflr, xip_ctrl_hw->stat);
			printf("MCU1 txftlr: %d, rxftlr: %d, baudr: %d\n", ssi_hw->txftlr, ssi_hw->rxftlr, ssi_hw->baudr);

			sleep_ms(1000);
		}
	} else {
		// I am MCU2

		// GPIO4 is our custom chip select
		gpio_init(CUSTOM_CSN_PIN);
		gpio_set_dir(CUSTOM_CSN_PIN, GPIO_IN);

		// Disable SSI and enable SIO (GPIO mode) for the QSPI pins
		// ssi_hw->ssienr = 0;

		for (int i = 0; i < 6; i++) {
			ioqspi_hw->io[i].ctrl = QSPI_GPIO_INPUT;
		}

		// Configure SSI to become a slave somehow
		while (true) {
			count++;
			gpio_put(LED_PIN, count & 1);	// Toggle LED slowly
			// ssi_hw->dr0 = count;

			printf("MCU2\n");

			while (gpio_get(CUSTOM_CSN_PIN)) {
				// Wait for CSn = 0
			}
			// printf("CSn=0\n");

			// Bit-bang receive SPI
			uint32_t stat0 = ioqspi_hw->io[QSPI_SCLK_PIN].status;
			while (!gpio_get(CUSTOM_CSN_PIN)) {
				// Wait for CSn = 1
				uint32_t stat1 = ioqspi_hw->io[QSPI_SCLK_PIN].status;
				if (stat0 != stat1) {
					printf("%08lX %08lX\n", stat0, stat1);
					stat0 = stat1;
				}
				// // wait for posedge SCK
				// while (!QSPI_GPIO_INPUT_GET(QSPI_SCLK_PIN)) {}
				// printf("SCK=1, MOSI=%d\n", !!QSPI_GPIO_INPUT_GET(QSPI_SD0_PIN));

				// // wait for negedge SCK
				// while (QSPI_GPIO_INPUT_GET(QSPI_SCLK_PIN)) {}
				// printf("SCK=0, MOSI=%d\n", !!QSPI_GPIO_INPUT_GET(QSPI_SD0_PIN));

			}

			while (!gpio_get(CUSTOM_CSN_PIN)) {
				// Wait for CSn = 1
			}
			printf("CSn=1\n");

			for (int i = 0; i < 100; i++) {
				ioqspi_hw->io[QSPI_SD1_PIN].ctrl = QSPI_GPIO_OUTPUT_HIGH;
				// sleep_ms(1);
				ioqspi_hw->io[QSPI_SD1_PIN].ctrl = QSPI_GPIO_OUTPUT_LOW;
				// sleep_ms(1);
			}
			ioqspi_hw->io[QSPI_SD1_PIN].ctrl = QSPI_GPIO_INPUT;

			printf("MCU2 rxflr: %d, txflr: %d, xip_ctrl_hw->stat: %d\n", ssi_hw->rxflr, ssi_hw->txflr, xip_ctrl_hw->stat);

		}

	}

	while (true) {
		count++;
		printf("Hello, world! I am MCU %d -- %d\n", mcu_id, count);
		gpio_put(LED_PIN, count & 1);	// Toggle LED slowly
		sleep_ms(1000);
	}
	return 0;
}
