#include "qspi_helper.h"

#include "hardware/gpio.h"

#include "stdio.h"
#include "utils.h"

#include "psram.h"
// #include "hardware/flash.h"

// #include "hardware/resets.h" // pico-sdk reset defines

// #include "gpio_helper.h"
// #define VERBOSE

bool hasSavedValues = false;
uint32_t pad_values[NUM_QSPI_GPIOS];
uint32_t pad_pull_values[NUM_QSPI_GPIOS];
void qspi_print_pull(void)
{
	for (int i = 0; i < NUM_QSPI_GPIOS; i++) {
		printf("%d ORIG:%08x NOW:%08X\n", i, pad_values[i], pads_qspi_hw->io[i]);
	}

	printf("SS %08x ... %08x\n", sio_hw->gpio_hi_out, sio_hw->gpio_hi_out & (1u << 1u));
}

void qspi_restore_to_startup_config() {
	for (int i = 0; i < NUM_QSPI_GPIOS; i++) {
		pads_qspi_hw->io[i] = pad_values[i];
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

// ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
// 										   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	// if (enable_ss) {
	// 	ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
	// 									   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	// } else {
	// 	ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
	// 									   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	// }

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

	if (!hasSavedValues) {
		for (int i = 0; i < NUM_QSPI_GPIOS; i++) {
			pad_values[i] = pads_qspi_hw->io[i];
		}
		hasSavedValues = true;
	}
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	// ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
	// 								   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));

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
	ssi->baudr = 4;

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

void qspi_init_qspi(bool sendQuadEnableCommand)
{
	if (sendQuadEnableCommand) {
		ssi->ssienr = 0;
		ssi->baudr = 8;
		ssi->ctrlr0 =
				(SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
				(7 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
				(SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
		ssi->spi_ctrlr0 =
				(0 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
				(2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
				(8u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
				(SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A  // Command in serial format address in quad
						<< SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
		ssi->ssienr = 1;

		psram_set_cs(1);
		ssi->dr0 = 0x35;
		psram_set_cs(0);

		// psram_set_cs(1);
		// ssi->dr0 = 0x000000;
		// psram_set_cs(0);

		while ((ssi_hw->sr & SSI_SR_BUSY_BITS) != 0) { tight_loop_contents(); }  

		printf("Sent quad mode enable command\n");  
		printf("Sample delay %d\n",ssi->rx_sample_dly);
	}

	

    ssi->ssienr = 0;
    ssi->baudr = 2;
    ssi->ctrlr0 =
            (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Quad SPI serial frames
            (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
            (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
    ssi->spi_ctrlr0 =
            (0xEB << SSI_SPI_CTRLR0_XIP_CMD_LSB) | 
            (4u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
            (SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |    // 
            (8u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
            (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_2C2A  // Command and address both in serial format
                    << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);

	ssi->rx_sample_dly = 2;
    ssi->ssienr = 1;

    printf("DONE!\n");
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
						(0 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
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
	qspi_oeover_normal(false); // want to use the CS line
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
		uint8_t chip = 3;//psram_addr_to_chip(address);
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
	uint32_t offset = 0;
	while (length > 0) {
		uint32_t count = (length > 64) ? 64 : length;
		uint32_t address_masked = address & 0xFFFFFF;
		uint8_t chip = 3;//psram_addr_to_chip(address);
#ifdef VERBOSE
		printf("RD[%d] @%08lX: %d bytes\n", chip, address_masked, count);
#endif
		qspi_put_cmd_addr(chip, FLASHCMD_READ_DATA, address_masked, 0); 
		qspi_put_get(chip, NULL, &data[offset], count);

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

void __no_inline_not_in_flash_func(qspi_flash_bulk_read)(uint32_t cmd, uint32_t *rxbuf, uint32_t flash_offs, size_t len,
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
	if (cmd != 0) {
    	ssi_hw->dr0 = (cmd << 24) | flash_offs;
	} else {
		// Read from flash
		ssi_hw->dr0 = (flash_offs << 8u) | 0xa0u;
	}

    // Wait for DMA finish
    while (dma_hw->ch[dma_chan].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS);

    // Reconfigure SSI before we jump back into flash!
    ssi_hw->ssienr = 0;
    ssi_hw->ctrlr1 = 0; // Single 32-bit data frame per transfer
    ssi_hw->dmacr = 0;
    ssi_hw->ssienr = 1;
}

// #define QSPI_SS_NORMAL 0x0
// #define QSPI_SS_INVERT 0x1
// #define QSPI_SS_LOW 0x2
// #define QSPI_SS_HIGH 0x3
// // force qspi ss into a high or low state
// static void __noinline flash_cs_force(int over) {
//     io_rw_32 *reg = (io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SS_CTRL_OFFSET);

// 	*reg = *reg & ~IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS
// 		| (over << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB);

//     // Read to flush async bridge
//     (void) *reg;
// }

// // Connect the XIP controller to the flash pads
// void __noinline qspi_connect_internal_flash() {
//     // Use hard reset to force IO and pad controls to known state (don't touch
//     // IO_BANK0 as that does not affect XIP signals)
//     //reset_unreset_block_wait_noinline(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS);
// 	reset_block(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS);

//     // Then mux XIP block onto internal QSPI flash pads
//     io_rw_32 *iobank1 = (io_rw_32 *) IO_QSPI_BASE;

//     for (int i = 0; i < 6; ++i) {
//         iobank1[2 * i + 1] = 0;
// 	}
// }

// // Flash init spi must be used to issue serial commands
// // Set up the SSI controller for standard SPI mode,i.e. for every byte sent we get one back
// // This is only called by flash_exit_xip(), not by any of the other functions.
// // This makes it possible for the debugger or user code to edit SPI settings
// // e.g. baud rate, CPOL/CPHA.
// void qspi_flash_init_spi() {
// 	//ssi->baudr = 2;
// 	printf("Init flash spi - 8bit\n");
// 	// Disable SSI for further config
//     ssi->ssienr = 0;
//     // Clear sticky errors (clear-on-read)
//     (void) ssi->sr;
//     (void) ssi->icr;
    
//     ssi->baudr = 2;
//     ssi->ctrlr0 =
//             (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) | // Standard 1-bit SPI serial frames
//             (7 << SSI_CTRLR0_DFS_32_LSB) | // 8 clocks per data frame
//             (SSI_CTRLR0_TMOD_VALUE_TX_AND_RX << SSI_CTRLR0_TMOD_LSB);  // TX and RX FIFOs are both used for every byte
//     // Slave selected when transfers in progress
//     ssi->ser = 1;
//     // Re-enable
//     ssi->ssienr = 1;
// }

// // Sequence:
// // 1. CSn = 1, IO = 4'h0 (via pulldown to avoid contention), x32 clocks
// // 2. CSn = 0, IO = 4'hf (via pullup to avoid contention), x32 clocks
// // 3. CSn = 1 (brief deassertion)
// // 4. CSn = 0, MOSI = 1'b1 driven, x16 clocks
// //
// // Part 4 is the sequence suggested in W25X10CL datasheet.
// // Parts 1 and 2 are to improve compatibility with Micron parts

// // GCC produces some heinous code if we try to loop over the pad controls,
// // so structs it is
// struct sd_padctrl {
//     io_rw_32 sd0;
//     io_rw_32 sd1;
//     io_rw_32 sd2;
//     io_rw_32 sd3;
// };
// void __noinline qspi_flash_exit_xip() {
//     struct sd_padctrl *qspi_sd_padctrl = (struct sd_padctrl *) (PADS_QSPI_BASE + PADS_QSPI_GPIO_QSPI_SD0_OFFSET);
//     io_rw_32 *qspi_ss_ioctrl = (io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SS_CTRL_OFFSET);
//     uint8_t buf[2];
//     buf[0] = 0xff;
//     buf[1] = 0xff;

//     qspi_flash_init_spi();

//     uint32_t padctrl_save = qspi_sd_padctrl->sd0;
//     uint32_t padctrl_tmp = (padctrl_save
//                             & ~(PADS_QSPI_GPIO_QSPI_SD0_OD_BITS | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS |
//                                 PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS)
//                            ) | PADS_QSPI_GPIO_QSPI_SD0_OD_BITS | PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS;

//     // First two 32-clock sequences
//     // CSn is held high for the first 32 clocks, then asserted low for next 32
//     // flash_cs_force(OUTOVER_HIGH);
// 	psram_set_cs(DEBUG_CS_CHIP_USE); // use our custom cs not the qspi one

//     for (int i = 0; i < 2; ++i) {
//         // This gives 4 16-bit offset store instructions. Anything else seems to
//         // produce a large island of constants
//         qspi_sd_padctrl->sd0 = padctrl_tmp;
//         qspi_sd_padctrl->sd1 = padctrl_tmp;
//         qspi_sd_padctrl->sd2 = padctrl_tmp;
//         qspi_sd_padctrl->sd3 = padctrl_tmp;

//         // Brief delay (~6000 cyc) for pulls to take effect
//         uint32_t delay_cnt = 1u << 11;
//         asm volatile (
//         "1: \n\t"
//         "sub %0, %0, #1 \n\t"
//         "bne 1b"
//         : "+r" (delay_cnt)
//         );

//         qspi_flash_put_get(NULL, NULL, 4, 0);

//         padctrl_tmp = (padctrl_tmp
//                        & ~PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS)
//                       | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS;

//         // flash_cs_force(OUTOVER_LOW);
// 		psram_set_cs(0);
//     }

//     // Restore IO/pad controls, and send 0xff, 0xff. Put pullup on IO2/IO3 as
//     // these may be used as WPn/HOLDn at this point, and we are now starting
//     // to issue serial commands.

//     qspi_sd_padctrl->sd0 = padctrl_save;
//     qspi_sd_padctrl->sd1 = padctrl_save;
//     padctrl_save = (padctrl_save
//         & ~PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS
//     ) | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS;
//     qspi_sd_padctrl->sd2 = padctrl_save;
//     qspi_sd_padctrl->sd3 = padctrl_save;

//     // flash_cs_force(OUTOVER_LOW);
// 	psram_set_cs(0);

//     qspi_flash_put_get(buf, NULL, 2, 0);

//     *qspi_ss_ioctrl = 0;
// }

// void __noinline qspi_flash_flush_cache() {
//     xip_ctrl_hw->flush = 1;
//     // Read blocks until flush completion
//     (void) xip_ctrl_hw->flush;
//     // Enable the cache
//     hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_EN_BITS);
	
// 	// We don't want to return the qspi cs to normal
//     //flash_cs_force(OUTOVER_NORMAL);
// }

// void qspi_flash_init() {
// 	//qspi_oeover_normal(false); // don't use the SS pin, we will control that
// 	// qspi_restore_to_startup_config();
// 	// flash_cs_force_low();
// 	// current_mcu_enable_demux(true);
// 	// xip_ctrl_hw->ctrl = (xip_ctrl_hw->ctrl & (~XIP_CTRL_EN_BITS));
// 	// xip_ctrl_hw->flush = 1;

// 	// uint8_t stat;
// 	// qspi_flash_do_cmd(DEBUG_CS_CHIP_USE, 0x05, NULL, &stat, 1);
// 	// printf("reg 1: %02x\n", stat);
// 	// qspi_flash_do_cmd(DEBUG_CS_CHIP_USE, 0x35, NULL, &stat, 1);
// 	// printf("reg 2: %02x\n", stat);

// 	flash_cs_force(QSPI_SS_HIGH); // turn off the qspi cs pin so we don't overwrite the program flash

// 	current_mcu_enable_demux(true);
// 	// qspi_restore_to_startup_config();
// 	//qspi_connect_internal_flash();
// 	qspi_oeover_normal(false);

// 	// printf("Exiting xip...\n");
// 	// qspi_flash_exit_xip();
// 	qspi_flash_init_spi();

// 	// qspi_flash_init_spi(); // this is called in flash exit xip
// 	printf("\nDONE!\n");
// }

// void helper_enable_qspi_flash_mode() {

// }

// void qspi_flash_enable_xip() {
// 	printf("Setup Flash XIP. CMD 0x6b\n");
// 	ssi->ssienr = 0;
// 	ssi->baudr = 2;
//     ssi->ctrlr0 =
//             (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
//             (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
//             (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
//     ssi->spi_ctrlr0 =
//             (0x6b << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
// 			(8u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
//             (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
//             (6u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
//             (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A  // Command and address both in serial format
//                     << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
//     ssi->ssienr = 1;
// }

// void qspi_flash_enable_xip2() {
// 	printf("Setup Flash XIP. CMD 0xeb\n");
// 	ssi->ssienr = 0;
// 	ssi->baudr = 2;
//     ssi->ctrlr0 =
//             (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
//             (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
//             (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
//     ssi->spi_ctrlr0 =
//             (0xeb << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
// 			(4u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
//             (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
//             (6u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
//             (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A  // Command and address both in serial format
//                     << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
//     ssi->ssienr = 1;
// }


// // This SHOULD enable quad reads but may or may not work. Seems that writes still aren't working correctly
// // void qspi_flash_enable_xip() {
// // 	// ssi->baudr = 2;
// // 	printf("Init flash spi - 32bit. Baud divider %d\n", ssi->baudr);

// // 	// Config from bootloader
// // 	ssi->ssienr = 0;
	
// // 	ssi_hw->ctrlr1 = 0;			// NDF=0 (single 32b read)

// // 	ssi->ctrlr0 =
// // 			(SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
// // 			(31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
// // 			(SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data

// // 	ssi->spi_ctrlr0 = 
// // 			(8 << SSI_SPI_CTRLR0_ADDR_L_LSB) |    /* Total number of address + mode bits */
// // 			(4 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |    /* Hi-Z dummy clocks following address + mode */
// // 			(SSI_SPI_CTRLR0_INST_L_VALUE_8B << SSI_SPI_CTRLR0_INST_L_LSB) |         /* Do not send a command, instead send XIP_CMD as mode bits after address */
// // 			(SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);     /* Send Command in serial mode then address in Quad I/O mode */

// // 	ssi->ssienr = 1;

// // // Sending this data or waiting for ready bit causes it to freeze
// // 	printf("Send fast read quad command and contious mode bits\n");
// // 	// uint8_t tx[] = {0x00, 0x00, 0x00, 0xa0};
// // 	qspi_flash_put_cmd_addr(DEBUG_CS_CHIP_USE, FLASHCMD_FAST_QUAD_READ, 0x000000a0);
// // 	qspi_flash_do_cmd(DEBUG_CS_CHIP_USE, FLASHCMD_FAST_QUAD_READ, NULL, NULL, 0);
// // 	// psram_set_cs(DEBUG_CS_CHIP_USE);
// //     // ssi->dr0 = FLASHCMD_FAST_QUAD_READ;
// // 	// qspi_flash_put_get(tx, NULL, 4, 1);
	
// // 	// wait
// // 	printf("Wating for ready bit...\n");
// // 	// qspi_wait_ready(DEBUG_CS_CHIP_USE);

// // 	printf("DONE!\nNow configure for bus access via xip\n");

// // 	// now we should be in xip quad mode so let's setup continuous read
// // 	ssi->ssienr = 0;

// // 	ssi->spi_ctrlr0 = (MODE_CONTINUOUS_READ << SSI_SPI_CTRLR0_XIP_CMD_LSB) | /* Mode bits to keep flash in continuous read mode */ \
// // 					(8 << SSI_SPI_CTRLR0_ADDR_L_LSB) |    /* Total number of address + mode bits */
// // 					(4 << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |    /* Hi-Z dummy clocks following address + mode */
// // 					(SSI_SPI_CTRLR0_INST_L_VALUE_NONE << SSI_SPI_CTRLR0_INST_L_LSB) |         /* Do not send a command, instead send XIP_CMD as mode bits after address */
// // 					(SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_2C2A << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);     /* Send Address in Quad I/O mode (and Command but that is zero bits long) */ \

// // 	ssi->ssienr = 1;

// // 	// I don't trust this
// // 	flash_cs_force(QSPI_SS_HIGH); // turn off the qspi cs pin so we don't overwrite the program flash
// // }


// #define FLASH_BLOCK_ERASE_CMD 0xD8
// #define FLASH_WRITE_ENABLE_CMD 0x06

// // Put bytes from one buffer, and get bytes into another buffer.
// // These can be the same buffer.
// // If tx is NULL then send zeroes.
// // If rx is NULL then all read data will be dropped.
// //
// // If rx_skip is nonzero, this many bytes will first be consumed from the FIFO,
// // before reading a further count bytes into *rx.
// // E.g. if you have written a command+address just before calling this function.
// void __noinline qspi_flash_put_get(const uint8_t *tx, uint8_t *rx, size_t count, size_t rx_skip) {
//     // Make sure there is never more data in flight than the depth of the RX
//     // FIFO. Otherwise, when we are interrupted for long periods, hardware
//     // will overflow the RX FIFO.
//     const uint max_in_flight = 16 - 2; // account for data internal to SSI
//     size_t tx_count = count;
//     size_t rx_count = count;
//     while (tx_count || rx_skip || rx_count) {
//         // NB order of reads, for pessimism rather than optimism
//         uint32_t tx_level = ssi_hw->txflr;
//         uint32_t rx_level = ssi_hw->rxflr;
//         bool did_something = false; // Expect this to be folded into control flow, not register
//         if (tx_count && tx_level + rx_level < max_in_flight) {
//             ssi->dr0 = (uint32_t) (tx ? *tx++ : 0);
//             --tx_count;
//             did_something = true;
//         }
//         if (rx_level) {
//             uint8_t rxbyte = ssi->dr0;
//             did_something = true;
//             if (rx_skip) {
//                 --rx_skip;
//             } else {
//                 if (rx)
//                     *rx++ = rxbyte;
//                 --rx_count;
//             }
//         }
//         // APB load costs 4 cycles, so only do it on idle loops (our budget is 48 cyc/byte)
//         if (!did_something)
//             break;
//     }
//     psram_set_cs(0);
// }

// // Timing of this one is critical, so do not expose the symbol to debugger etc
// static inline void qspi_flash_put_cmd_addr(uint8_t chip, uint8_t cmd, uint32_t addr) {
//     psram_set_cs(chip);
//     addr |= cmd << 24;
//     for (int i = 0; i < 4; ++i) {
//         ssi->dr0 = addr >> 24;
//         addr <<= 8;
//     }
// }

// // Convenience wrapper for above
// // (And it's hard for the debug host to get the tight timing between
// // cmd DR0 write and the remaining data)
// void qspi_flash_do_cmd(uint8_t chip, uint8_t cmd, const uint8_t *tx, uint8_t *rx, size_t count) {
//     psram_set_cs(chip);
//     ssi->dr0 = cmd;
//     qspi_flash_put_get(tx, rx, count, 1);
// }

// // Set the WEL bit (needed before any program/erase operation)
// static __noinline void qspi_flash_enable_write() {
//     qspi_flash_do_cmd(DEBUG_CS_CHIP_USE, FLASH_WRITE_ENABLE_CMD, NULL, NULL, 0);
// }

// // Poll the flash status register until the busy bit (LSB) clears
// static inline void qspi_flash_wait_ready(uint8_t chip) {
//     uint8_t stat;
//     do {
//         qspi_flash_do_cmd(chip, FLASHCMD_READ_STATUS, NULL, &stat, 1);
//     } while (stat & 0x1);
// }

// void t(const uint8_t *txbuf, uint8_t *rxbuf, size_t count) {
// 	psram_set_cs(1);
//     size_t tx_remaining = count;
//     size_t rx_remaining = count;
//     // We may be interrupted -- don't want FIFO to overflow if we're distracted.
//     const size_t max_in_flight = 16 - 2;
//     while (tx_remaining || rx_remaining) {
//         uint32_t flags = ssi_hw->sr;
//         bool can_put = !!(flags & SSI_SR_TFNF_BITS);
//         bool can_get = !!(flags & SSI_SR_RFNE_BITS);
//         if (can_put && tx_remaining && rx_remaining - tx_remaining < max_in_flight) {
//             ssi_hw->dr0 = *txbuf++;
//             --tx_remaining;
//         }
//         if (can_get && rx_remaining) {
//             *rxbuf++ = (uint8_t)ssi_hw->dr0;
//             --rx_remaining;
//         }
//     }
//     psram_set_cs(0);
// }

// // Easier to call erase on the whole chip than 
// void qspi_flash_erase_block(uint32_t address) {
// 	// enable write bit

// 	// uint8_t stat[4];
// 	// qspi_flash_put_cmd_addr(1, 0x90, 0);
// 	// qspi_flash_put_get(NULL, stat, 2, 1);
// 	// printf("id: %02x %02x\n", stat[0], stat[1]);

// 	uint8_t txbuf[4] = {0x90, 0, 0, 0};
//     uint8_t rxbuf[4] = {0};
// 	t(txbuf, rxbuf, 4);
// 	printf("id: %02x %02x %02x %02x\n", rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3]);

// 	// qspi_flash_enable_write();

// 	// // send erase command and address
// 	// qspi_flash_put_cmd_addr(DEBUG_CS_CHIP_USE, FLASH_BLOCK_ERASE_CMD, address);

// 	// // in pico bootrom code fo erase, it does this before waiting for the status bit
// 	// qspi_flash_put_get(NULL, NULL, 0, 4);

// 	// // wait for status bit
// 	// qspi_flash_wait_ready(DEBUG_CS_CHIP_USE);
// }

// void qspi_flash_write(uint32_t address, uint8_t * data, uint32_t length) {
// 	qspi_flash_enable_write();

// 	qspi_flash_put_cmd_addr(DEBUG_CS_CHIP_USE, FLASHCMD_WRITE, address);
// 	qspi_flash_put_get(data, NULL, length, 4);
	
// 	// Wait for busy bit to clear
// 	qspi_flash_wait_ready(DEBUG_CS_CHIP_USE);
// }

// void qspi_flash_read_data(uint32_t addr, uint8_t *rx, size_t count) {
//     qspi_flash_put_cmd_addr(DEBUG_CS_CHIP_USE, FLASHCMD_READ_DATA, addr);
//     qspi_flash_put_get(NULL, rx, count, 4);
// }