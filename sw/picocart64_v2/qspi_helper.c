#include "qspi_helper.h"

#include "hardware/gpio.h"

#include "stdio.h"

#include "pins_mcu2.h"
#include "gpio_helper.h"
#include "utils.h"

#include "psram_inline.h"

#define VERBOSE

void qspi_print_pull(void)
{
	for (int i = 0; i < NUM_QSPI_GPIOS; i++) {
		printf("%d %08X\n", i, pads_qspi_hw->io[i]);
	}
}

void qspi_set_pull(bool disabled, bool pullup, bool pulldown)
{
	/*
	   Default bitmask:

	   0 00000021
	   1 00000050
	   2 00000050
	   3 00000050
	   4 00000050
	   5 0000005A
	 */

	pullup = false;
	pulldown = true;

	uint32_t values[6];
	uint32_t od = disabled ? 1UL : 0UL;
	uint32_t pue = pullup ? 1UL : 0UL;
	uint32_t pde = pulldown ? 1UL : 0UL;

	values[QSPI_SCLK_PAD] =
		(od << PADS_QSPI_GPIO_QSPI_SCLK_OD_LSB) |
		(1 << PADS_QSPI_GPIO_QSPI_SCLK_IE_LSB) |
		(3 << PADS_QSPI_GPIO_QSPI_SCLK_DRIVE_LSB) |
		(1 << PADS_QSPI_GPIO_QSPI_SCLK_PUE_LSB) |
		(0 << PADS_QSPI_GPIO_QSPI_SCLK_PDE_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SCHMITT_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SLEWFAST_LSB);

	// Common values for SD0, SD1, SD2, SD3
	for (int i = 0; i < 4; i++) {
		values[QSPI_SD0_PAD + i] =
			(od << PADS_QSPI_GPIO_QSPI_SCLK_OD_LSB) |
			(1 << PADS_QSPI_GPIO_QSPI_SCLK_IE_LSB) |
			(0 << PADS_QSPI_GPIO_QSPI_SCLK_DRIVE_LSB) |
			(0 << PADS_QSPI_GPIO_QSPI_SCLK_PUE_LSB) |
			(1 << PADS_QSPI_GPIO_QSPI_SCLK_PDE_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SCHMITT_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SLEWFAST_LSB);
	}

	values[QSPI_SS_PAD] =
		(od << PADS_QSPI_GPIO_QSPI_SCLK_OD_LSB) |
		(1 << PADS_QSPI_GPIO_QSPI_SCLK_IE_LSB) |
		(0 << PADS_QSPI_GPIO_QSPI_SCLK_DRIVE_LSB) |
		(1 << PADS_QSPI_GPIO_QSPI_SCLK_PUE_LSB) |
		(0 << PADS_QSPI_GPIO_QSPI_SCLK_PDE_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SCHMITT_LSB) | (0 << PADS_QSPI_GPIO_QSPI_SCLK_SLEWFAST_LSB);

	for (int i = 0; i < NUM_QSPI_GPIOS; i++) {
		printf("%d %08X -> %08X\n", i, pads_qspi_hw->io[i], values[i]);
		pads_qspi_hw->io[i] = values[i];
	}
}

void qspi_oeover_normal(bool enable_ss)
{
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	if (enable_ss) {
		ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
										   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	} else {
		ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
										   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	}

	ioqspi_hw->io[QSPI_SD0_PIN].ctrl = (ioqspi_hw->io[QSPI_SD1_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD1_PIN].ctrl = (ioqspi_hw->io[QSPI_SD2_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD2_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD3_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_LSB));

	qspi_set_pull(false, false, false);
}

void qspi_oeover_disable(void)
{
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
									   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD0_PIN].ctrl = (ioqspi_hw->io[QSPI_SD1_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD1_PIN].ctrl = (ioqspi_hw->io[QSPI_SD2_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD2_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD3_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_LSB));

	qspi_set_pull(true, false, true);
}

///////////////

// These are supported by almost any SPI flash
#define FLASHCMD_WRITE            0x02
#define FLASHCMD_QUAD_WRITE       0x38

#define FLASHCMD_READ_DATA        0x03
#define FLASHCMD_FAST_READ        0x0B
#define FLASHCMD_FAST_QUAD_READ   0xEB

#define FLASHCMD_READ_STATUS      0x05

static ssi_hw_t *const ssi = (ssi_hw_t *) XIP_SSI_BASE;

// Set up the SSI controller for standard SPI mode,i.e. for every byte sent we get one back
// This is only called by flash_exit_xip(), not by any of the other functions.
// This makes it possible for the debugger or user code to edit SPI settings
// e.g. baud rate, CPOL/CPHA.
void qspi_init_spi(void)
{
	// Disable SSI for further config
	ssi->ssienr = 0;
	// Clear sticky errors (clear-on-read)
	(void)ssi->sr;
	(void)ssi->icr;

	// ssi->baudr = 2;
	// ssi->baudr = 6;
	ssi->baudr = 20;

	ssi->ctrlr0 = (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |	// Standard 1-bit SPI serial frames
		(7 << SSI_CTRLR0_DFS_32_LSB) |	// 8 clocks per data frame
		(SSI_CTRLR0_TMOD_VALUE_TX_AND_RX << SSI_CTRLR0_TMOD_LSB);	// TX and RX FIFOs are both used for every byte
	// Slave selected when transfers in progress
	ssi->ser = 1;
	// Re-enable
	ssi->ssienr = 1;
}

void qspi_init_qspi(void)
{
	// Disable SSI for further config
	ssi->ssienr = 0;
	// Clear sticky errors (clear-on-read)
	(void)ssi->sr;
	(void)ssi->icr;

	// ssi->baudr = 2;
	// ssi->baudr = 6;
	ssi->baudr = 20;

	// ssi->ctrlr0 = (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |    // ...
	//  (7 << SSI_CTRLR0_DFS_32_LSB) |  // 8 clocks per data frame, i.e. 4 bytes
	//  (0 << SSI_CTRLR0_CFS_LSB) | //
	//  (SSI_CTRLR0_TMOD_VALUE_TX_AND_RX << SSI_CTRLR0_TMOD_LSB);   // TX and RX FIFOs are both used for every byte

	ssi->ctrlr0 = ((0 << SSI_CTRLR0_SSTE_LSB) |	//
				   (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |	// 
				   (31 << SSI_CTRLR0_DFS_32_LSB) |	// 
				   (0 << SSI_CTRLR0_CFS_LSB) |	// 
				   (0 << SSI_CTRLR0_SRL_LSB) |	// 
				   (0 << SSI_CTRLR0_SLV_OE_LSB) |	// 
				   (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB) |	//
				   (0 << SSI_CTRLR0_SCPOL_LSB) |	// 
				   (0 << SSI_CTRLR0_SCPH_LSB) |	// 
				   (0 << SSI_CTRLR0_FRF_LSB) |	// 
				   (0 << SSI_CTRLR0_DFS_LSB)	//
		);

	ssi->ctrlr1 = 0;

	ssi->spi_ctrlr0 = ((0x00 << SSI_SPI_CTRLR0_XIP_CMD_LSB) |	//
					   (0 << SSI_SPI_CTRLR0_SPI_RXDS_EN_LSB) |	//
					   (0 << SSI_SPI_CTRLR0_INST_DDR_EN_LSB) |	//
					   (0 << SSI_SPI_CTRLR0_SPI_DDR_EN_LSB) |	//
					   (2 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |	/* Hi-Z dummy clocks following address + mode */
					   (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |	/*  */
					   (0 << SSI_SPI_CTRLR0_ADDR_L_LSB) |	/* Total number of address + mode bits */
					   (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB)	/* Send Address in Quad I/O mode (and Command but that is zero bits long) */
		);

	// Slave selected when transfers in progress
	ssi->ser = 1;
	// Re-enable
	ssi->ssienr = 1;
}

static void empty_tx_fifo(uint8_t limit)
{
	// Busy-wait while the TX fifo is greater than the limit
	while (ssi_hw->txflr > limit) {
	}
}

static void empty_rx_fifo(void)
{
	uint32_t rx_level;
	do {
		rx_level = ssi_hw->rxflr;
		if (rx_level > 0) {
			uint32_t junk = ssi_hw->dr0;
#ifdef VERBOSE
			printf("rx_level: %d, junk: %08x\n", rx_level, junk);
#endif
		}
	} while (rx_level > 0);
}

// Put bytes from one buffer, and get bytes into another buffer.
// These can be the same buffer.
// If tx is NULL then send zeroes.
// If rx is NULL then all read data will be dropped.
static void __noinline qspi_put_get(uint8_t chip, const uint8_t * tx, uint8_t * rx, size_t count)
{
	uint8_t rxbyte;

	empty_tx_fifo(0);
	empty_rx_fifo();

	while (count > 0) {
		ssi->dr0 = (uint32_t) (tx ? *tx++ : 0);
		--count;

		// Wait until there is 1 byte in the RX fifo
		while (ssi_hw->rxflr == 0) {
		}

		rxbyte = ssi->dr0;
		if (rx) {
			*rx++ = rxbyte;
		}
		// TODO: Could have something > 0 here, but why risk it.
		empty_tx_fifo(0);
	}

	psram_set_cs(0);
}

static void __noinline qspi_put_get_qspi(uint8_t chip, const uint8_t * tx, uint8_t * rx, size_t count)
{
	uint8_t rxbyte;
	uint32_t rxword;
	uint32_t *rx32 = (uint32_t *) rx;

	empty_tx_fifo(0);
	empty_rx_fifo();

	if (count % 4 != 0) {
		printf("Uhhhhhhhh\n");
		while (1) {
		}
	}

	while (count > 0) {
		ssi->dr0 = (uint32_t) (tx ? *tx++ : 0);
		count -= 4;

		// Wait until there is 1 byte in the RX fifo
		while (ssi_hw->rxflr == 0) {
		}

		// rxbyte = ssi->dr0;
		rxword = ssi->dr0;
		// printf("%08X ", rxword);
		if (rx32) {
			*rx32++ = rxword;
		}
		// TODO: Could have something > 0 here, but why risk it.
		empty_tx_fifo(0);
	}

	psram_set_cs(0);
}

void qspi_do_cmd(uint8_t chip, uint8_t cmd, const uint8_t * tx, uint8_t * rx, size_t count)
{
	psram_set_cs(chip);
	ssi->dr0 = cmd;
	empty_tx_fifo(0);
	empty_rx_fifo();
	qspi_put_get(chip, tx, rx, count);
}

// Poll the flash status register until the busy bit (LSB) clears
static inline void qspi_wait_ready(uint8_t chip)
{
	uint8_t stat;
	do {
		qspi_do_cmd(chip, FLASHCMD_READ_STATUS, NULL, &stat, 1);
	} while (stat & 0x1);
}

// dummy: Number of dummy bytes to send and receive (throw away)
static inline void qspi_put_cmd_addr(uint8_t chip, uint8_t cmd, uint32_t addr, uint8_t dummy)
{
	empty_tx_fifo(0);
	empty_rx_fifo();

	psram_set_cs(chip);

	addr |= cmd << 24;
	for (int i = 0; i < 4; ++i) {
		ssi->dr0 = addr >> 24;
		addr <<= 8;

		// Wait until there is 1 byte in the RX FIFO
		while (ssi_hw->rxflr == 0) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo();
	}

	for (int i = 0; i < dummy; i++) {
		ssi->dr0 = 0;

		// Wait until there is 1 byte in the RX FIFO
		while (ssi_hw->rxflr == 0) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo();
	}
}

static inline void qspi_put_cmd_addr_qspi(uint8_t chip, uint8_t cmd, uint32_t addr, uint8_t dummy)
{
	empty_tx_fifo(0);
	empty_rx_fifo();

	psram_set_cs(chip);

	// First send the SPI command, translated to all four wires
	// 0b87654321 =>
	// D0            0b87 65 43 21
	// D1            0b00 00 00 00
	// D2            0b00 00 00 00
	// D3            0b00 00 00 00
	//
	// 87654321
	//  * * * *
	// 
	uint8_t quad_bits = cmd;
	uint32_t combined = (((quad_bits << (0 * 4 - 0)) & 0x00000001) |	// Bit 0
						 ((quad_bits << (1 * 4 - 1)) & 0x00000010) |	// Bit 1
						 ((quad_bits << (2 * 4 - 2)) & 0x00000100) |	// Bit 2
						 ((quad_bits << (3 * 4 - 3)) & 0x00001000) |	// Bit 3
						 ((quad_bits << (4 * 4 - 4)) & 0x00010000) |	// Bit 4
						 ((quad_bits << (5 * 4 - 5)) & 0x00100000) |	// Bit 5
						 ((quad_bits << (6 * 4 - 6)) & 0x01000000) |	// Bit 6
						 ((quad_bits << (7 * 4 - 7)) & 0x10000000)	// Bit 7
		);

	ssi->dr0 = 0x01;
	ssi->dr0 = 0x10;
	ssi->dr0 = 0x100;
	ssi->dr0 = 0x1000;

	// ssi->dr0 = 0x01010101;
	// ssi->dr0 = 0x01010101;
	// ssi->dr0 = 0x01010101;
	// ssi->dr0 = 0x01010101;
	// ssi->dr0 = 0xFFFFFFFF;
	// ssi->dr0 = 0x55;
	// ssi->dr0 = 0xFFFFFFFF;
	// ssi->dr0 = 0xFFFFFFFF;
	// ssi->dr0 = 0xFFFFFFFF;
	// ssi->dr0 = 0xFFFFFFFF;
	// ssi->dr0 = combined;
	//ssi->dr0 = 0xFFFFFFFF;

	// Wait until there are 4 bytes in the RX FIFO
	while (ssi_hw->rxflr < 1) {
	}

	empty_tx_fifo(0);
	empty_rx_fifo();

	printf("*** halt ***\n");
	while (1) {
	}

	/////////////////////////////
	// Write addr << 8
	ssi->dr0 = addr << 8;

	// Wait until there are 4 bytes in the RX FIFO
	while (ssi_hw->rxflr < 1) {
	}

	empty_tx_fifo(0);
	empty_rx_fifo();

	/////////////////////////////
	// Write dummy
	for (int i = 0; i < dummy; i++) {
		ssi->dr0 = 0;

		// Wait until there are 4 bytes in the RX FIFO
		while (ssi_hw->rxflr < 1) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo();
	}
}

#define FRAME_FORMAT SSI_CTRLR0_SPI_FRF_VALUE_QUAD
#define MODE_CONTINUOUS_READ 0xa0

// The number of address + mode bits, divided by 4 (always 4, not function of
// interface width).
#define ADDR_L 8

#define WAIT_CYCLES 4

// Put the SSI into a mode where XIP accesses translate to standard
// serial 03h read commands. The flash remains in its default serial command
// state, so will still respond to other commands.
void qspi_enter_cmd_xip(void)
{
	ssi->ssienr = 0;

	// Clear sticky errors (clear-on-read)
	(void)ssi_hw->sr;
	(void)ssi_hw->icr;

	// ssi_hw->baudr = 2;
	// ssi_hw->baudr = 4;
	ssi_hw->baudr = 20;

	ssi_hw->ctrlr0 = ((0 << SSI_CTRLR0_SSTE_LSB) |	// 
					  (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |	// 
					  (31 << SSI_CTRLR0_DFS_32_LSB) |	// 
					  (0 << SSI_CTRLR0_CFS_LSB) |	// 
					  (0 << SSI_CTRLR0_SRL_LSB) |	// 
					  (0 << SSI_CTRLR0_SLV_OE_LSB) |	// 
					  (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB) |	// 
					  (0 << SSI_CTRLR0_SCPOL_LSB) |	//
					  (0 << SSI_CTRLR0_SCPH_LSB) |	// 
					  (0 << SSI_CTRLR0_FRF_LSB) |	// 
					  (0 << SSI_CTRLR0_DFS_LSB)	// 
		);
	ssi_hw->ctrlr1 = 0;			// NDF=0 (single 32b read)
	/* 
	   This peripheral seems to be buggy!
	   It seems that SSI_SPI_CTRLR0_ADDR_L_LSB and SSI_SPI_CTRLR0_WAIT_CYCLES_LSB
	   only has an effect when not in SSI_CTRLR0_SPI_FRF_VALUE_STD mode. Weird.
	 */
	ssi_hw->spi_ctrlr0 = ((FLASHCMD_READ_DATA << SSI_SPI_CTRLR0_XIP_CMD_LSB) |	// (0 << SSI_SPI_CTRLR0_SPI_RXDS_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_INST_DDR_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_SPI_DDR_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |    /* Hi-Z dummy clocks following address + mode */
						  (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |	/*  */
						  (8 << SSI_SPI_CTRLR0_ADDR_L_LSB) |	/* Total number of address + mode bits */
						  (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB)	/* Send Address in Quad I/O mode (and Command but that is zero bits long) */
		);
	// ssi_hw->rx_sample_dly = 8;
	ssi_hw->ssienr = 1;
	// ssi->ctrlr0 = (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |    // Standard 4-bit SPI serial frames
	//  (31 << SSI_CTRLR0_DFS_32_LSB) | // 32 clocks per data frame
	//  (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
	// ssi->spi_ctrlr0 = 
	//  (8u << SSI_SPI_CTRLR0_ADDR_L_LSB) | // 24-bit addressing for 03h commands
	//  (6u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
	//  (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |
	//  (0xEB << SSI_SPI_CTRLR0_XIP_CMD_LSB) |
	//  (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB)
	// ;
	// ssi_hw->ctrlr1 = 0; // NDF=0 (single 32b read)
	// ssi->ssienr = 1;
}

// DEMUX enable
static const gpio_config_t mcu2_demux_enabled_config[] = {
	// Demux should be configured as inputs without pulls until we lock the bus
	{PIN_DEMUX_A0, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A1, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A2, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_IE, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
};

// DEMUX disable
static const gpio_config_t mcu2_demux_disabled_config[] = {
	// Demux should be configured as inputs without pulls until we lock the bus
	{PIN_DEMUX_A0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_A2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
	{PIN_DEMUX_IE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO},
};

void qspi_demux_enable(bool enabled)
{

	if (enabled) {
		gpio_configure(mcu2_demux_enabled_config, ARRAY_SIZE(mcu2_demux_enabled_config));
	} else {
		gpio_configure(mcu2_demux_disabled_config, ARRAY_SIZE(mcu2_demux_disabled_config));
	}
}

void qspi_enable(void)
{
	qspi_oeover_normal(false);
	qspi_demux_enable(true);
	qspi_init_spi();
	// Turn off the XIP cache
	xip_ctrl_hw->ctrl = (xip_ctrl_hw->ctrl & (~XIP_CTRL_EN_BITS));
	xip_ctrl_hw->flush = 1;
}

void qspi_disable(void)
{
	qspi_demux_enable(false);
	qspi_oeover_disable();
}

void qspi_write(uint32_t address, const uint8_t * data, uint32_t length)
{
	uint32_t offset = 0;
	while (length > 0) {
		uint32_t count = length > 64 ? 64 : length;
		uint32_t address_masked = address & 0xFFFFFF;
		uint8_t chip = psram_addr_to_chip(address);
#ifdef VERBOSE
		printf("PP[%d] @%08lX: %d bytes\n", chip, address_masked, count);
		printf("  ");
		for (int i = 0; i < count; i++) {
			printf("%02X ", data[offset + i]);
		}
		printf("\n");
#endif
		qspi_put_cmd_addr(chip, FLASHCMD_WRITE, address_masked, 0);
		qspi_put_get(chip, &data[offset], NULL, count);
		// maybe skip this?
		// qspi_wait_ready(chip);
		offset += count;
		address += count;
		length -= count;
	}
}

void qspi_read(uint32_t address, uint8_t * data, uint32_t length)
{
	// TODO: This is still broken!!
	uint32_t offset = 0;
	while (length > 0) {
		uint32_t count = (length > 64) ? 64 : length;
		uint32_t address_masked = address & 0xFFFFFF;
		uint8_t chip = psram_addr_to_chip(address);
#ifdef VERBOSE
		printf("RD[%d] @%08lX: %d bytes\n", chip, address_masked, count);
#endif
		// qspi_put_cmd_addr(chip, FLASHCMD_FAST_READ, address_masked, 1);
		// qspi_put_cmd_addr(chip, FLASHCMD_READ_DATA, address_masked, 0);
		// qspi_put_cmd_addr(chip, FLASHCMD_READ_DATA, address_masked, 0);
		qspi_put_cmd_addr_qspi(chip, FLASHCMD_FAST_QUAD_READ, address_masked, 0);
		qspi_put_get_qspi(chip, NULL, &data[offset], count);
#ifdef VERBOSE
		printf("  ");
		for (int i = 0; i < count; i++) {
			printf("%02X ", data[offset + i]);
		}
		printf("\n");
#endif
		offset += count;
		address += count;
		length -= count;
	}
}

// #define DATA_LEN 1024*8
#define DATA_LEN 256
uint32_t data[DATA_LEN];
void qspi_test(void)
{

	printf("\n1. ------------\n\n");
	for (int i = 0; i < DATA_LEN; i++) {
		// data[i] = i;
		// data[i] = 0x41;
		// data[i] = 0x00410000 + i;
		// data[i] = 0x004100ff - i;
		// data[i] = 0x0;
		// data[i] = 0xaabbccdd;
		data[i] = 0xaabbcc00 + i;
		// data[i] = 0xa0b0c0d0 + (i << 0) + (i << 8) + (i << 16) + (i << 24);
	}

	printf("\n2. ------------\n\n");
	qspi_enable();
	printf("\n3. ------------\n\n");
	qspi_write(0, data, DATA_LEN * 4);
	for (int i = 0; i < DATA_LEN; i++) {
		data[i] = 0x11223344;
	}

	vTaskDelay(5 * 1000);
	printf("\n4. ------------\n\n");
	qspi_init_qspi();
	qspi_read(0, data, DATA_LEN * 4);
	for (int i = 0; i < DATA_LEN; i++) {
		if (data[i] != i) {
			printf("ERROR %08X != %08X\n", data[i], i);
		}
	}

	// for (int i = 0; i < DATA_LEN; i++) {
	for (int i = 0; i < 8; i++) {
		printf("%d: %08X\n", i, data[i]);
	}

	while (1) {
		printf("\n\n**** HALTuuu \n");
		vTaskDelay(5 * 1000);
	}

	printf("\n5. ------------\n\n");
	qspi_disable();
	printf("\n6. ------------\n\n");
}
