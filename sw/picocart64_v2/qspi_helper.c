#include "qspi_helper.h"

#include "hardware/gpio.h"

#include "stdio.h"
#include "utils.h"

#include "psram.h"

// #include "gpio_helper.h"
// #define VERBOSE

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
		#if VERBOSE
			printf("%d %08X -> %08X\n", i, pads_qspi_hw->io[i], values[i]);
		#endif
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

#define PSRAM_ENTER_QUAD_MODE	  0x35

static ssi_hw_t *const ssi = (ssi_hw_t *) XIP_SSI_BASE;

static void empty_tx_fifo(uint8_t limit)
{
	// Busy-wait while the TX fifo is greater than the limit
	while (ssi_hw->txflr > limit) {
	}
}

static void empty_rx_fifo(bool print)
{
	uint32_t rx_level;
	do {
		rx_level = ssi_hw->rxflr;
		if (rx_level > 0) {
			uint32_t junk = ssi_hw->dr0;
// #ifdef VERBOSE
if (print) {
			printf("rx_level: %d, junk: %08x\n", rx_level, junk);
}
// #endif
		}
	} while (rx_level > 0);
}


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

// Put bytes from one buffer, and get bytes into another buffer.
// These can be the same buffer.
// If tx is NULL then send zeroes.
// If rx is NULL then all read data will be dropped.
static void __noinline qspi_put_get(uint8_t chip, const uint8_t * tx, uint8_t * rx, size_t count)
{
	uint8_t rxbyte;

	empty_tx_fifo(0);
	empty_rx_fifo(false);

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
	empty_rx_fifo(false);

	if (count % 4 != 0) {
		printf("!!ERROR: qspi_put_get_qspi count must be a multiple of 4!!\n");
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

static uint32_t wait_and_read(uint8_t count) {
    while ((ssi_hw->sr & SSI_SR_TFE_BITS) == 0) {}
    while ((ssi_hw->sr & SSI_SR_BUSY_BITS) != 0) {}
    uint32_t result = 0;
    while (count > 0) {
        result = ssi_hw->dr0;
        count--;
    }
    return result;
}

void qspi_do_cmd(uint8_t chip, uint8_t cmd, const uint8_t * tx, uint8_t * rx, size_t count)
{
	psram_set_cs(chip);
	ssi->dr0 = cmd;
	empty_tx_fifo(0);
	empty_rx_fifo(false);
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
	empty_rx_fifo(false);

	psram_set_cs(chip);

	addr |= cmd << 24;
	for (int i = 0; i < 4; ++i) {
		ssi->dr0 = addr >> 24;
		addr <<= 8;

		// Wait until there is 1 byte in the RX FIFO
		while (ssi_hw->rxflr == 0) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo(false);
	}

	for (int i = 0; i < dummy; i++) {
		ssi->dr0 = 0;

		// Wait until there is 1 byte in the RX FIFO
		while (ssi_hw->rxflr == 0) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo(false);
	}
}

void qspi_init_qspi(void)
{

	printf("Setting up SSI_QSPI\n");

	// qspi_put_cmd_addr(1, PSRAM_ENTER_QUAD_MODE, 0, 0);
	// First enable quad mode, spi should be working before this
	// probably want regular xip mode before we enter into this mode
	// printf("Sending ENTER QUAD MODE command\n");
	// qspi_put_cmd_addr(1, PSRAM_ENTER_QUAD_MODE, 0, 0);
	// psram_set_cs(0);

	// Disable SSI for further config
	ssi_hw->ssienr = 0;
	// Clear sticky errors (clear-on-read)
	(void)ssi_hw->sr;
	(void)ssi_hw->icr;

	// ssi->baudr = 2;
	// ssi->baudr = 6;
	ssi->baudr = 20;

	// This value doesn't make sense. Some places say clocks per data frame and other are bits per data frame
	//SSI_CTRLR0_DFS_32_LSB
	// If it's bits per clock it's 4, if it's clocks per byte it's 2 (need to value-1 because reasons)
	// I can see using 1 (i.e. 2 as in 2 clocks per byte)
	// OR 7 (ie.e 8 as in 8 bits per byte?)
	// 31 (i.e. 32) doesn't make sense unless that means we just want to transfer in and our 32 bits values

	ssi_hw->ctrlr0 = ((SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |	// 
				   (31 << SSI_CTRLR0_DFS_32_LSB) |	
				   (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB)
		);

	ssi_hw->ctrlr1 = 0;			// NDF=0 (single 32b read)

	/*
	FROM THE BOOT LOADER ASSEMBLY
	// Note that the INST_L field is used to select what XIP data gets pushed into
	// the TX FIFO:
	//      INST_L_0_BITS   {ADDR[23:0],XIP_CMD[7:0]}       Load "mode bits" into XIP_CMD
	//      Anything else   {XIP_CMD[7:0],ADDR[23:0]}       Load SPI command into XIP_CMD
	// So in this case SSI_SPI_CTRLR0_ADDR_L_LSB should be 8 = (8 bit command + 24 bit address)/4
	*/

	// 4 wait cycles for "fast read" 0x0B when in quad mode
	// 8 wait cycles for "fast read" 0x0B when in spi mode / quad read
	// 6 wait cycles for "fast read quad" 0xEB when in quad mode
	ssi_hw->spi_ctrlr0 = ((FLASHCMD_FAST_READ << SSI_SPI_CTRLR0_XIP_CMD_LSB) |	//
					   (8 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |	/* Hi-Z dummy clocks following address + mode */
					   (6 << SSI_SPI_CTRLR0_ADDR_L_LSB) |	/* Total number of address + mode bits, this is also supposed to include the instruction if you aren't using mode bits? So it's either 6 (24bit address) or 8 (24bit address + 8bit command)*/
					   (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |	/* Instruction is 8 bits  */
					   (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB)	/* Command and address both in standard SPI mode */
		);

	// Slave selected when transfers in progress
	ssi_hw->ser = 1;
	// Re-enable
	ssi_hw->ssienr = 1;
}

static inline void qspi_put_cmd_addr_qspi(uint8_t chip, uint8_t cmd, uint32_t addr, uint8_t dummy)
{
	empty_tx_fifo(0);
	empty_rx_fifo(false);

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

	// ssi->dr0 = 0x01;
	// ssi->dr0 = 0x10;
	// ssi->dr0 = 0x100;
	// ssi->dr0 = 0x1000;

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
	empty_rx_fifo(false);

	/////////////////////////////
	// Write addr << 8
	ssi->dr0 = addr << 8;

	// Wait until there are 4 bytes in the RX FIFO
	while (ssi_hw->rxflr < 1) {
	}

	empty_tx_fifo(0);
	empty_rx_fifo(false);

	/////////////////////////////
	// Write dummy
	for (int i = 0; i < dummy; i++) {
		ssi->dr0 = 0;

		// Wait until there are 4 bytes in the RX FIFO
		while (ssi_hw->rxflr < 1) {
		}

		empty_tx_fifo(0);
		empty_rx_fifo(false);
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
	ssi_hw->baudr = 4;

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
	
	ssi_hw->ssienr = 1;
}

void qspi_enter_cmd_xip_with_params(uint8_t cmd, bool quad_addr, bool quad_data, int dfs, int addr_l, int waitCycles, int baudDivider) {
	ssi->ssienr = 0;

	// Clear sticky errors (clear-on-read)
	(void)ssi_hw->sr;
	(void)ssi_hw->icr;

	// ssi_hw->baudr = 2;
	// ssi_hw->baudr = 4;
	ssi_hw->baudr = baudDivider;

	int frf = quad_data ? SSI_CTRLR0_SPI_FRF_VALUE_QUAD : SSI_CTRLR0_SPI_FRF_VALUE_STD;

	// Field       : SSI_SPI_CTRLR0_TRANS_TYPE
	// Description : Address and instruction transfer format
	//               0x0 -> Command and address both in standard SPI frame format
	//               0x1 -> Command in standard SPI format, address in format
	//               specified by FRF
	//               0x2 -> Command and address both in format specified by FRF
	//               (e.g. Dual-SPI)
	// if quad_addr then send command in standard spi format and address in format specifed by frf 
	// else command and address are both standard spi
	int trans_type = quad_addr ? SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A : SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A;
	
	ssi_hw->ctrlr0 = ((0 << SSI_CTRLR0_SSTE_LSB) |	// 
					  (frf << SSI_CTRLR0_SPI_FRF_LSB) |	// 
					  (dfs << SSI_CTRLR0_DFS_32_LSB) |	// 
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

	ssi_hw->spi_ctrlr0 = ((cmd << SSI_SPI_CTRLR0_XIP_CMD_LSB) |	// (0 << SSI_SPI_CTRLR0_SPI_RXDS_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_INST_DDR_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_SPI_DDR_EN_LSB) | /**/(0 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |    /* Hi-Z dummy clocks following address + mode */
						  (waitCycles << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
						  (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |	/*  */
						  (addr_l << SSI_SPI_CTRLR0_ADDR_L_LSB) |	/* Total number of address + mode bits */
						  (trans_type << SSI_SPI_CTRLR0_TRANS_TYPE_LSB)	/* Send Address in Quad I/O mode (and Command but that is zero bits long) */
		);
	
	ssi_hw->ssienr = 1;

	printf("cmd:%02x, frf:%02x, trans_type:%02x, dfs:%d, addr_l:%d, waitCycles: %d, baudDivider:%d\n", cmd, frf, trans_type, dfs, addr_l, waitCycles, baudDivider);
}

void print_bits(uint32_t value, uint32_t start, uint32_t end) {

	int hexValue = 0x0;
	int numPrints = end-start+1;
	for(int i = 0; i < 8; i++) {
		if (i < numPrints) {
			int v = (value >> start+i) & 1;
			hexValue+=(v * (i+1));
			printf("%d ", v);
		} else {
			printf("  "); // print some space to keep everything neatly aligned
		}
	}
	printf(" => %x\n", hexValue);
}

void dump_current_ssi_config() {
	io_rw_32 baud = ssi_hw->baudr;
	io_rw_32 ctrlr0 = ssi_hw->ctrlr0;
	io_rw_32 ctrlr1 = ssi_hw->ctrlr1;
	io_rw_32 spi_ctrlr0 = ssi_hw->spi_ctrlr0;

	printf("CURRENT SSI CONFIG\n");
	printf("-----------------------------------\n");
	printf("baud:       %08x\n", baud);
	printf("ctrlr0:     %08x\n", ctrlr0);
	printf("ctrlr1:     %08x\n", ctrlr1);
	printf("spi_ctrlr0: %08x\n", spi_ctrlr0);

    // Control register 0
    // 0x01000000 [24]    : SSTE (0): Slave select toggle enable
    // 0x00600000 [22:21] : SPI_FRF (0): SPI frame format
    // 0x001f0000 [20:16] : DFS_32 (0): Data frame size in 32b transfer mode
    // 0x0000f000 [15:12] : CFS (0): Control frame size
    // 0x00000800 [11]    : SRL (0): Shift register loop (test mode)
    // 0x00000400 [10]    : SLV_OE (0): Slave output enable
    // 0x00000300 [9:8]   : TMOD (0): Transfer mode
    // 0x00000080 [7]     : SCPOL (0): Serial clock polarity
    // 0x00000040 [6]     : SCPH (0): Serial clock phase
    // 0x00000030 [5:4]   : FRF (0): Frame format
    // 0x0000000f [3:0]   : DFS (0): Data frame size
	printf("\nctrlr0 values\n");
	printf("SSTE:       "); print_bits(ctrlr0, 24, 24); 
	printf("SPI_FRF:    "); print_bits(ctrlr0, 21, 22); 
	printf("DFS_32:     "); print_bits(ctrlr0, 16, 20); 
	printf("CFS:        "); print_bits(ctrlr0, 12, 15); 
	printf("SRL:        "); print_bits(ctrlr0, 11, 11); 
	printf("SLV_OE:     "); print_bits(ctrlr0, 10, 10); 
	printf("TMOD:       "); print_bits(ctrlr0, 8, 9); 
	printf("SCPOL:      "); print_bits(ctrlr0, 7, 7); 
	printf("SCPH:       "); print_bits(ctrlr0, 6, 6); 
	printf("FRF:        "); print_bits(ctrlr0, 4, 5); 
	printf("DFS:        "); print_bits(ctrlr0, 0, 3); 

	// SPI control
    // 0xff000000 [31:24] : XIP_CMD (0x3): SPI Command to send in XIP mode (INST_L = 8-bit) or to append to Address (INST_L = 0-bit)
    // 0x00040000 [18]    : SPI_RXDS_EN (0): Read data strobe enable
    // 0x00020000 [17]    : INST_DDR_EN (0): Instruction DDR transfer enable
    // 0x00010000 [16]    : SPI_DDR_EN (0): SPI DDR transfer enable
    // 0x0000f800 [15:11] : WAIT_CYCLES (0): Wait cycles between control frame transmit and data reception (in SCLK cycles)
    // 0x00000300 [9:8]   : INST_L (0): Instruction length (0/4/8/16b)
    // 0x0000003c [5:2]   : ADDR_L (0): Address length (0b-60b in 4b increments)
    // 0x00000003 [1:0]   : TRANS_TYPE (0): Address and instruction transfer format
	printf("\nspi_ctrlr0 values\n");
	printf("XIP_CMD:    "); print_bits(spi_ctrlr0, 24, 31); 
	printf("SPI_RXDS_EN:"); print_bits(spi_ctrlr0, 18, 18); 
	printf("INST_DDR_EN:"); print_bits(spi_ctrlr0, 17, 17); 
	printf("SPI_DDR_EN: "); print_bits(spi_ctrlr0, 16, 16); 
	printf("WAIT_CYCLES:"); print_bits(spi_ctrlr0, 11, 15); 
	printf("INST_L:     "); print_bits(spi_ctrlr0, 8, 9); 
	printf("ADDR_L:     "); print_bits(spi_ctrlr0, 2, 5); 
	printf("TRANS_TYPE: "); print_bits(spi_ctrlr0, 0, 1); 

	printf("-----------------------------------\n");
}

void qspi_enable()
{
	qspi_oeover_normal(false);
	current_mcu_enable_demux(true);
	qspi_init_spi();
	// Turn off the XIP cache
	xip_ctrl_hw->ctrl = (xip_ctrl_hw->ctrl & (~XIP_CTRL_EN_BITS));
	xip_ctrl_hw->flush = 1;
}

void qspi_disable(const gpio_config_t *demuxConfig)
{
	current_mcu_enable_demux(false);
	qspi_oeover_disable();
}

int lastChipUsed = 0;
void qspi_write(uint32_t address, const uint8_t * data, uint32_t length)
{
	uint32_t offset = 0;
	while (length > 0) {
		uint32_t count = length > 64 ? 64 : length;
		uint32_t address_masked = address & 0xFFFFFF;
		uint8_t chip = psram_addr_to_chip(address);
		if (lastChipUsed != chip) {
			printf("CHIP[%d]", chip);
			lastChipUsed = chip;
		}
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

void __no_inline_not_in_flash_func(flash_bulk_read)(uint32_t *rxbuf, uint32_t flash_offs, size_t len,
                                                 uint dma_chan) {
    // SSI must be disabled to set transfer size. If software is executing
    // from flash right now then it's about to have a bad time
    ssi_hw->ssienr = 0;
    ssi_hw->ctrlr1 = len - 1; // NDF, number of data frames
    ssi_hw->dmacr = SSI_DMACR_TDMAE_BITS | SSI_DMACR_RDMAE_BITS;
    ssi_hw->ssienr = 1;
    // Other than NDF, the SSI configuration used for XIP is suitable for a bulk read too.

    // Configure and start the DMA. Note we are avoiding the dma_*() functions
    // as we can't guarantee they'll be inlined
    dma_hw->ch[dma_chan].read_addr = (uint32_t) &ssi_hw->dr0;
    dma_hw->ch[dma_chan].write_addr = (uint32_t) rxbuf;
    dma_hw->ch[dma_chan].transfer_count = len;
    // Must enable DMA byteswap because non-XIP 32-bit flash transfers are
    // big-endian on SSI (we added a hardware tweak to make XIP sensible)
    dma_hw->ch[dma_chan].ctrl_trig =
            DMA_CH0_CTRL_TRIG_BSWAP_BITS |
            DREQ_XIP_SSIRX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB |
            dma_chan << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB |
            DMA_CH0_CTRL_TRIG_INCR_WRITE_BITS |
            DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_WORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB |
            DMA_CH0_CTRL_TRIG_EN_BITS;

    // Now DMA is waiting, kick off the SSI transfer (mode continuation bits in LSBs)
    ssi_hw->dr0 = 0x0e | (flash_offs << 8u);//| 0xa0u;

    // Wait for DMA finish
    while (dma_hw->ch[dma_chan].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS);

    // Reconfigure SSI before we jump back into flash!
    ssi_hw->ssienr = 0;
    ssi_hw->ctrlr1 = 0; // Single 32-bit data frame per transfer
    ssi_hw->dmacr = 0;
    ssi_hw->ssienr = 1;
}

void qspi_read(uint32_t address, uint8_t * data, uint32_t length)
{
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
		
		// psram_set_cs(chip);
		// flash_bulk_read((uint32_t*)data, 0, length/4, 0);
		// psram_set_cs(0);

		// This works //////////////////////////////////////////////////////////////
		qspi_put_cmd_addr(chip, FLASHCMD_READ_DATA, address_masked, 0); 
		qspi_put_get(chip, NULL, &data[offset], count);
		////////////////////////////////////////////////////////////////////////////

		// qspi_put_cmd_addr(chip, FLASHCMD_FAST_READ, address_masked, 0);  // this also seems to work but the first byte is empty?
		// qspi_put_cmd_addr(chip, FLASHCMD_FAST_QUAD_READ, address_masked, 0); // this doesn't stall but seems to return junk

		// qspi_put_cmd_addr_qspi(chip, FLASHCMD_FAST_QUAD_READ, address_masked, 0);
		// qspi_put_cmd_addr(chip, FLASHCMD_FAST_QUAD_READ, address_masked, 0);
		// qspi_put_get_qspi(chip, NULL, &data[offset], count);

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
// #define DATA_LEN 256
// uint32_t data[DATA_LEN];
// void qspi_test(void)
// {

// 	printf("\n1. ------------\n\n");
// 	for (int i = 0; i < DATA_LEN; i++) {
// 		// data[i] = i;
// 		// data[i] = 0x41;
// 		// data[i] = 0x00410000 + i;
// 		// data[i] = 0x004100ff - i;
// 		// data[i] = 0x0;
// 		// data[i] = 0xaabbccdd;
// 		data[i] = 0xaabbcc00 + i;
// 		// data[i] = 0xa0b0c0d0 + (i << 0) + (i << 8) + (i << 16) + (i << 24);
// 	}

// 	printf("\n2. ------------\n\n");
// 	qspi_enable();
// 	printf("\n3. ------------\n\n");
// 	qspi_write(0, data, DATA_LEN * 4);
// 	for (int i = 0; i < DATA_LEN; i++) {
// 		data[i] = 0x11223344;
// 	}

// 	vTaskDelay(5 * 1000);
// 	printf("\n4. ------------\n\n");
// 	qspi_init_qspi();
// 	qspi_read(0, data, DATA_LEN * 4);
// 	for (int i = 0; i < DATA_LEN; i++) {
// 		if (data[i] != i) {
// 			printf("ERROR %08X != %08X\n", data[i], i);
// 		}
// 	}

// 	// for (int i = 0; i < DATA_LEN; i++) {
// 	for (int i = 0; i < 8; i++) {
// 		printf("%d: %08X\n", i, data[i]);
// 	}

// 	while (1) {
// 		printf("\n\n**** HALTuuu \n");
// 		vTaskDelay(5 * 1000);
// 	}

// 	printf("\n5. ------------\n\n");
// 	qspi_disable();
// 	printf("\n6. ------------\n\n");
// }
