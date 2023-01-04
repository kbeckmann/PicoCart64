/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "hardware/regs/io_qspi.h"
#include "hardware/regs/pads_qspi.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/resets.h"
#include "program_flash_array.h"
#include "hardware/resets.h"

// These are supported by almost any SPI flash
#define FLASHCMD_PAGE_PROGRAM     0x02
#define FLASHCMD_READ_DATA        0x03
#define FLASHCMD_READ_STATUS      0x05
#define FLASHCMD_WRITE_ENABLE     0x06
#define FLASHCMD_SECTOR_ERASE     0x20
#define FLASHCMD_READ_SFDP        0x5a
#define FLASHCMD_READ_JEDEC_ID    0x9f

// Annoyingly, structs give much better code generation, as they re-use the base
// pointer rather than doing a PC-relative load for each constant pointer.

static ssi_hw_t *const ssi = (ssi_hw_t *) XIP_SSI_BASE;

// Sanity check
#undef static_assert
#define static_assert(cond, x) extern int static_assert[(cond)?1:-1]
check_hw_layout(ssi_hw_t, ssienr, SSI_SSIENR_OFFSET);
check_hw_layout(ssi_hw_t, spi_ctrlr0, SSI_SPI_CTRLR0_OFFSET);

// ----------------------------------------------------------------------------
// Setup and generic access functions

// Connect the XIP controller to the flash pads
void __noinline program_connect_internal_flash() {
    // Use hard reset to force IO and pad controls to known state (don't touch
    // IO_BANK0 as that does not affect XIP signals)
    // reset_unreset_block_wait_noinline(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS);
    reset_block(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS);
    unreset_block_wait(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS);

    // Then mux XIP block onto internal QSPI flash pads
    io_rw_32 *iobank1 = (io_rw_32 *) IO_QSPI_BASE;
#ifndef GENERAL_SIZE_HACKS
    for (int i = 0; i < 6; ++i)
        iobank1[2 * i + 1] = 0;
#else
    __asm volatile (
    "str %1, [%0, #4];" \
        "str %1, [%0, #12];" \
        "str %1, [%0, #20];" \
        "str %1, [%0, #28];" \
        "str %1, [%0, #36];" \
        "str %1, [%0, #44];"
    ::"r" (iobank1), "r" (0));
#endif
}

io_rw_32 grab_ctrlr0() {
    uint32_t config = ssi->ctrlr0;
    return config;
}

io_rw_32 grab_spi_ctrlr_0() {
    uint32_t config = ssi->spi_ctrlr0;
    return config;
}

void program_flash_disable() {
    ssi->ssienr = 0;
}

// Set up the SSI controller for standard SPI mode,i.e. for every byte sent we get one back
// This is only called by flash_exit_xip(), not by any of the other functions.
// This makes it possible for the debugger or user code to edit SPI settings
// e.g. baud rate, CPOL/CPHA.
void program_flash_init_spi() {
    // Disable SSI for further config
    ssi->ssienr = 0;
    // Clear sticky errors (clear-on-read)
    (void) ssi->sr;
    (void) ssi->icr;
    // Hopefully-conservative baud rate for boot and programming
    ssi->baudr = 6;
    ssi->ctrlr0 =
            (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) | // Standard 1-bit SPI serial frames
            (7 << SSI_CTRLR0_DFS_32_LSB) | // 8 clocks per data frame
            (SSI_CTRLR0_TMOD_VALUE_TX_AND_RX << SSI_CTRLR0_TMOD_LSB);  // TX and RX FIFOs are both used for every byte
    // Slave selected when transfers in progress
    ssi->ser = 1;
    // Re-enable
    ssi->ssienr = 1;
}

typedef enum {
    OUTOVER_NORMAL = 0,
    OUTOVER_INVERT,
    OUTOVER_LOW,
    OUTOVER_HIGH
} outover_t;

// Flash code may be heavily interrupted (e.g. if we are running USB MSC
// handlers concurrently with flash programming) so we control the CS pin
// manually
static void __noinline program_flash_cs_force(outover_t over) {
    io_rw_32 *reg = (io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SS_CTRL_OFFSET);
#ifndef GENERAL_SIZE_HACKS
    *reg = *reg & ~IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS
        | (over << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB);
#else
    // The only functions we want are FSEL (== 0 for XIP) and OUTOVER!
    *reg = over << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB;
#endif
    // Read to flush async bridge
    (void) *reg;
}

// Put bytes from one buffer, and get bytes into another buffer.
// These can be the same buffer.
// If tx is NULL then send zeroes.
// If rx is NULL then all read data will be dropped.
//
// If rx_skip is nonzero, this many bytes will first be consumed from the FIFO,
// before reading a further count bytes into *rx.
// E.g. if you have written a command+address just before calling this function.
void __noinline program_flash_put_get(const uint8_t *tx, uint8_t *rx, size_t count, size_t rx_skip) {
    // Make sure there is never more data in flight than the depth of the RX
    // FIFO. Otherwise, when we are interrupted for long periods, hardware
    // will overflow the RX FIFO.
    const uint max_in_flight = 16 - 2; // account for data internal to SSI
    size_t tx_count = count;
    size_t rx_count = count;
    while (tx_count || rx_skip || rx_count) {
        // NB order of reads, for pessimism rather than optimism
        uint32_t tx_level = ssi_hw->txflr;
        uint32_t rx_level = ssi_hw->rxflr;
        bool did_something = false; // Expect this to be folded into control flow, not register
        if (tx_count && tx_level + rx_level < max_in_flight) {
            ssi->dr0 = (uint32_t) (tx ? *tx++ : 0);
            --tx_count;
            did_something = true;
        }
        if (rx_level) {
            uint8_t rxbyte = ssi->dr0;
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
        if (!did_something && __builtin_expect(program_flash_was_aborted(), 0))
            break;
    }
    program_flash_cs_force(OUTOVER_HIGH);
}

// Convenience wrapper for above
// (And it's hard for the debug host to get the tight timing between
// cmd DR0 write and the remaining data)
void program_flash_do_cmd(uint8_t cmd, const uint8_t *tx, uint8_t *rx, size_t count) {
    program_flash_cs_force(OUTOVER_LOW);
    ssi->dr0 = cmd;
    program_flash_put_get(tx, rx, count, 1);
}


// Timing of this one is critical, so do not expose the symbol to debugger etc
static inline void program_flash_put_cmd_addr(uint8_t cmd, uint32_t addr) {
    program_flash_cs_force(OUTOVER_LOW);
    addr |= cmd << 24;
    for (int i = 0; i < 4; ++i) {
        ssi->dr0 = addr >> 24;
        addr <<= 8;
    }
}

// GCC produces some heinous code if we try to loop over the pad controls,
// so structs it is
struct sd_padctrl {
    io_rw_32 sd0;
    io_rw_32 sd1;
    io_rw_32 sd2;
    io_rw_32 sd3;
};

// Sequence:
// 1. CSn = 1, IO = 4'h0 (via pulldown to avoid contention), x32 clocks
// 2. CSn = 0, IO = 4'hf (via pullup to avoid contention), x32 clocks
// 3. CSn = 1 (brief deassertion)
// 4. CSn = 0, MOSI = 1'b1 driven, x16 clocks
//
// Part 4 is the sequence suggested in W25X10CL datasheet.
// Parts 1 and 2 are to improve compatibility with Micron parts

void __noinline program_flash_exit_xip() {
    struct sd_padctrl *qspi_sd_padctrl = (struct sd_padctrl *) (PADS_QSPI_BASE + PADS_QSPI_GPIO_QSPI_SD0_OFFSET);
    io_rw_32 *qspi_ss_ioctrl = (io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SS_CTRL_OFFSET);
    uint8_t buf[2];
    buf[0] = 0xff;
    buf[1] = 0xff;

    program_flash_init_spi();

    uint32_t padctrl_save = qspi_sd_padctrl->sd0;
    uint32_t padctrl_tmp = (padctrl_save
                            & ~(PADS_QSPI_GPIO_QSPI_SD0_OD_BITS | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS |
                                PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS)
                           ) | PADS_QSPI_GPIO_QSPI_SD0_OD_BITS | PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS;

    // First two 32-clock sequences
    // CSn is held high for the first 32 clocks, then asserted low for next 32
    program_flash_cs_force(OUTOVER_HIGH);
    for (int i = 0; i < 2; ++i) {
        // This gives 4 16-bit offset store instructions. Anything else seems to
        // produce a large island of constants
        qspi_sd_padctrl->sd0 = padctrl_tmp;
        qspi_sd_padctrl->sd1 = padctrl_tmp;
        qspi_sd_padctrl->sd2 = padctrl_tmp;
        qspi_sd_padctrl->sd3 = padctrl_tmp;

        // Brief delay (~6000 cyc) for pulls to take effect
        uint32_t delay_cnt = 1u << 11;
        asm volatile (
        "1: \n\t"
        "sub %0, %0, #1 \n\t"
        "bne 1b"
        : "+r" (delay_cnt)
        );

        program_flash_put_get(NULL, NULL, 4, 0);

        padctrl_tmp = (padctrl_tmp
                       & ~PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS)
                      | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS;

        program_flash_cs_force(OUTOVER_LOW);
    }

    // Restore IO/pad controls, and send 0xff, 0xff. Put pullup on IO2/IO3 as
    // these may be used as WPn/HOLDn at this point, and we are now starting
    // to issue serial commands.

    qspi_sd_padctrl->sd0 = padctrl_save;
    qspi_sd_padctrl->sd1 = padctrl_save;
    padctrl_save = (padctrl_save
        & ~PADS_QSPI_GPIO_QSPI_SD0_PDE_BITS
    ) | PADS_QSPI_GPIO_QSPI_SD0_PUE_BITS;
    qspi_sd_padctrl->sd2 = padctrl_save;
    qspi_sd_padctrl->sd3 = padctrl_save;

    program_flash_cs_force(OUTOVER_LOW);
    program_flash_put_get(buf, NULL, 2, 0);

    *qspi_ss_ioctrl = 0;
}

// ----------------------------------------------------------------------------
// Programming

// Poll the flash status register until the busy bit (LSB) clears
static inline void program_flash_wait_ready() {
    uint8_t stat;
    do {
        program_flash_do_cmd(FLASHCMD_READ_STATUS, NULL, &stat, 1);
    } while (stat & 0x1 && !program_flash_was_aborted());
}

// Set the WEL bit (needed before any program/erase operation)
static __noinline void program_flash_enable_write() {
    program_flash_do_cmd(FLASHCMD_WRITE_ENABLE, NULL, NULL, 0);
}

// Program a 256 byte page at some 256-byte-aligned flash address,
// from some buffer in memory. Blocks until completion.
void program_flash_page_program(uint32_t addr, const uint8_t *data) {
    assert(addr < 0x1000000);
    assert(!(addr & 0xffu));
    program_flash_enable_write();
    program_flash_put_cmd_addr(FLASHCMD_PAGE_PROGRAM, addr);
    program_flash_put_get(data, NULL, 256, 4);
    program_flash_wait_ready();
}

void program_write_buf(uint32_t addr, const uint8_t* data, uint32_t len) {
    program_flash_put_cmd_addr(FLASHCMD_PAGE_PROGRAM, addr);
    program_flash_put_get(data, NULL, len, 4);
}

// Program a range of flash with some data from memory.
// Size is rounded up to nearest 256 bytes.
void __noinline program_flash_range_program(uint32_t addr, const uint8_t *data, size_t count) {
    assert(!(addr & 0xffu));
    uint32_t goal = addr + count;
    while (addr < goal && !program_flash_was_aborted()) {
        program_flash_page_program(addr, data);
        addr += 256;
        data += 256;
    }
}

// Force MISO input to SSI low so that an in-progress SR polling loop will
// fall through. This is needed when a flash programming task in async task
// context is locked up (e.g. if there is no flash device, and a hard pullup
// on MISO pin -> SR read gives 0xff) and the host issues an abort in IRQ
// context. Bit of a hack
void program_flash_abort() {
    hw_set_bits(
            (io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SD1_CTRL_OFFSET),
            IO_QSPI_GPIO_QSPI_SD1_CTRL_INOVER_VALUE_LOW << IO_QSPI_GPIO_QSPI_SD1_CTRL_INOVER_LSB
    );
}

// Also allow any unbounded loops to check whether the above abort condition
// was asserted, and terminate early
int program_flash_was_aborted() {
    return *(io_rw_32 *) (IO_QSPI_BASE + IO_QSPI_GPIO_QSPI_SD1_CTRL_OFFSET)
           & IO_QSPI_GPIO_QSPI_SD1_CTRL_INOVER_BITS;
}

// ----------------------------------------------------------------------------
// Erase

// Setting correct address alignment is the caller's responsibility

// Use some other command, supplied by user e.g. a block erase or a chip erase.
// Despite the name, the user is not erased by this function.
void program_flash_user_erase(uint32_t addr, uint8_t cmd) {
    assert(addr < 0x1000000);
    program_flash_enable_write();
    program_flash_put_cmd_addr(cmd, addr);
    program_flash_put_get(NULL, NULL, 0, 4);
    program_flash_wait_ready();
}

// Use a standard 20h 4k erase command:
void program_flash_sector_erase(uint32_t addr) {
    program_flash_user_erase(addr, FLASHCMD_SECTOR_ERASE);
}

// block_size must be a power of 2.
// Generally block_size > 4k, and block_cmd is some command which erases a block
// of this size. This accelerates erase speed.
// To use sector-erase only, set block_size to some value larger than flash,
// e.g. 1ul << 31.
// To override the default 20h erase cmd, set block_size == 4k.
void __noinline program_flash_range_erase(uint32_t addr, size_t count, uint32_t block_size, uint8_t block_cmd) {
    uint32_t goal = addr + count;
    while (addr < goal && !program_flash_was_aborted()) {
        if (!(addr & (block_size - 1)) && goal - addr >= block_size) {
            program_flash_user_erase(addr, block_cmd);
            addr += block_size;
        } else {
            program_flash_sector_erase(addr);
            addr += 1ul << 12;
        }
    }
}

// ----------------------------------------------------------------------------
// Read

void __noinline program_flash_read_data(uint32_t addr, uint8_t *rx, size_t count) {
    assert(addr < 0x1000000);
    program_flash_put_cmd_addr(FLASHCMD_READ_DATA, addr);
    program_flash_put_get(NULL, rx, count, 4);
}

// ----------------------------------------------------------------------------
// Size determination via SFDP or JEDEC ID (best effort)
// Relevant XKCD is 927

static inline void program_flash_read_sfdp(uint32_t addr, uint8_t *rx, size_t count) {
    assert(addr < 0x1000000);
    program_flash_put_cmd_addr(FLASHCMD_READ_SFDP, addr);
    ssi->dr0 = 0; // dummy byte
    program_flash_put_get(NULL, rx, count, 5);
}

static inline __attribute__((always_inline)) uint32_t bytes_to_u32le(const uint8_t *b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

// Return value >= 0: log 2 of flash size in bytes.
// Return value < 0: unable to determine size.
int __noinline program_flash_size_log2() {
    uint8_t rxbuf[16];

    // Check magic
    program_flash_read_sfdp(0, rxbuf, 16);
    if (bytes_to_u32le(rxbuf) != ('S' | ('F' << 8) | ('D' << 16) | ('P' << 24)))
        goto sfdp_fail;
    // Skip NPH -- we don't care about nonmandatory parameters.
    // Offset 8 is header for mandatory parameter table
    // | ID | MinRev | MajRev | Length in words | ptr[2] | ptr[1] | ptr[0] | unused|
    // ID must be 0 (JEDEC) for mandatory PTH
    if (rxbuf[8] != 0)
        goto sfdp_fail;

    uint32_t param_table_ptr = bytes_to_u32le(rxbuf + 12) & 0xffffffu;
    program_flash_read_sfdp(param_table_ptr, rxbuf, 8);
    uint32_t array_size_word = bytes_to_u32le(rxbuf + 4);
    // MSB set: array >= 2 Gbit, encoded as log2 of number of bits
    // MSB clear: array < 2 Gbit, encoded as direct bit count
    if (array_size_word & (1u << 31)) {
        array_size_word &= ~(1u << 31);
    } else {
        uint32_t ctr = 0;
        array_size_word += 1;
        while (array_size_word >>= 1)
            ++ctr;
        array_size_word = ctr;
    }
    // Sanity check... 2kbit is minimum for 2nd stage, 128 Gbit is 1000x bigger than we can XIP
    if (array_size_word < 11 || array_size_word > 37)
        goto sfdp_fail;
    return array_size_word - 3;

    sfdp_fail:
    // If no SFDP, it's common to encode log2 of main array size in second
    // byte of JEDEC ID
    program_flash_do_cmd(FLASHCMD_READ_JEDEC_ID, NULL, rxbuf, 3);
    uint8_t array_size_byte = rxbuf[2];
    // Confusingly this is log2 of size in bytes, not bits like SFDP. Sanity check:
    if (array_size_byte < 8 || array_size_byte > 34)
        goto jedec_id_fail;
    return array_size_byte;

    jedec_id_fail:
    return -1;
}

// ----------------------------------------------------------------------------
// XIP Entry

// This is a hook for steps to be taken in between programming the flash and
// doing cached XIP reads from the flash. Called by the bootrom before
// entering flash second stage, and called by the debugger after flash
// programming.
void inline program_flash_flush_cache() {
    xip_ctrl_hw->flush = 1;
    // Read blocks until flush completion
    (void) xip_ctrl_hw->flush;
    // Enable the cache
    hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_EN_BITS);
    program_flash_cs_force(OUTOVER_NORMAL);
}

// Put the SSI into a mode where XIP accesses translate to standard
// serial 03h read commands. The flash remains in its default serial command
// state, so will still respond to other commands.
void __noinline program_flash_enter_cmd_xip() {
    // DEFAULT THAT WAS HERE
    ssi->ssienr = 0;
    ssi->ctrlr0 =
            (SSI_CTRLR0_SPI_FRF_VALUE_STD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
            (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
            (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
    ssi->spi_ctrlr0 =
            (FLASHCMD_READ_DATA << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
            (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
            (6u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
            (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A  // Command and address both in serial format
                    << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
    ssi->ssienr = 1;

// THIS WORKS!
    // ssi->ssienr = 0;
    // ssi->baudr = 2;
    // ssi->ctrlr0 =
    //         (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
    //         (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
    //         (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
    // ssi->spi_ctrlr0 =
    //         (0x6B << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
    //         (8u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
    //         (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
    //         (6u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
    //         (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C1A  // Command and address both in serial format
    //                 << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
    // ssi->ssienr = 1;


// FLASH QUAD MODE XIP - works
    // printf("Configure xip...\n");

    // ssi->ssienr = 0;
    // ssi->baudr = 2;
    // ssi->ctrlr0 =
    //         (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
    //         (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
    //         (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
    // ssi->spi_ctrlr0 =
    //         (4u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
    //         (2u << SSI_SPI_CTRLR0_INST_L_LSB) |    // 8-bit instruction prefix
    //         (8u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
    //         (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_1C2A  // Command in serial format address in quad
    //                 << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
    // ssi->ssienr = 1;

    // ssi->dr0 = 0xEB;
    // ssi->dr0 = 0x000000a0;

    // while ((ssi_hw->sr & SSI_SR_BUSY_BITS) != 0) { tight_loop_contents(); }  
    // printf("Sent read commands now setting up continous read mode\n");  

    // ssi->ssienr = 0;
    // ssi->baudr = 2;
    // ssi->ctrlr0 =
    //         (SSI_CTRLR0_SPI_FRF_VALUE_QUAD << SSI_CTRLR0_SPI_FRF_LSB) |  // Standard 1-bit SPI serial frames
    //         (31 << SSI_CTRLR0_DFS_32_LSB) |                             // 32 clocks per data frame
    //         (SSI_CTRLR0_TMOD_VALUE_EEPROM_READ << SSI_CTRLR0_TMOD_LSB); // Send instr + addr, receive data
    // ssi->spi_ctrlr0 =
    //         (0xa0 << SSI_SPI_CTRLR0_XIP_CMD_LSB) | // Standard 03h read
    //         (4u << SSI_SPI_CTRLR0_WAIT_CYCLES_LSB) |
    //         (SSI_SPI_CTRLR0_INST_L_VALUE_NONE << SSI_SPI_CTRLR0_INST_L_LSB) |    // 
    //         (8u << SSI_SPI_CTRLR0_ADDR_L_LSB) |    // 24-bit addressing for 03h commands
    //         (SSI_SPI_CTRLR0_TRANS_TYPE_VALUE_2C2A  // Command and address both in serial format
    //                 << SSI_SPI_CTRLR0_TRANS_TYPE_LSB);
    // ssi->ssienr = 1;

    // printf("DONE!\n");
}
