/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>

//#include "pico/bootrom.h"

#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

#include "flash_array.h"
#include "program_flash_array.h"
#include "psram.h"

#define FLASH_BLOCK_ERASE_CMD 0xd8

// Standard RUID instruction: 4Bh command prefix, 32 dummy bits, 64 data bits.
#define FLASH_RUID_CMD 0x4b
#define FLASH_RUID_DUMMY_BYTES 4
#define FLASH_RUID_DATA_BYTES 8
#define FLASH_RUID_TOTAL_BYTES (1 + FLASH_RUID_DUMMY_BYTES + FLASH_RUID_DATA_BYTES)

//-----------------------------------------------------------------------------
// Infrastructure for reentering XIP mode after exiting for programming (take
// a copy of boot2 before XIP exit). Calling boot2 as a function works because
// it accepts a return vector in LR (and doesn't trash r4-r7). Bootrom passes
// NULL in LR, instructing boot2 to enter flash vector table's reset handler.

#if FALSE //!PICO_NO_FLASH

#define BOOT2_SIZE_WORDS 64

static uint32_t boot2_copyout[BOOT2_SIZE_WORDS];
static bool boot2_copyout_valid = false;

void __no_inline_not_in_flash_func(picocart_flash_init_boot2_copyout)(void) {
    if (boot2_copyout_valid)
        return;
    for (int i = 0; i < BOOT2_SIZE_WORDS; ++i)
        boot2_copyout[i] = ((uint32_t *)XIP_BASE)[i];
    __compiler_memory_barrier();
    boot2_copyout_valid = true;
}

void __no_inline_not_in_flash_func(picocart_flash_enable_xip_via_boot2)(void) {
    ((void (*)(void))boot2_copyout+1)();
}

#else

void __no_inline_not_in_flash_func(picocart_flash_init_boot2_copyout)(void) {}

void __no_inline_not_in_flash_func(picocart_flash_enable_xip_via_boot2)(void) {
    // Set up XIP for 03h read on bus access (slow but generic)
    // rom_flash_enter_cmd_xip_fn flash_enter_cmd_xip = (rom_flash_enter_cmd_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_ENTER_CMD_XIP);
    // assert(flash_enter_cmd_xip);
    // flash_enter_cmd_xip();
    program_flash_enter_cmd_xip();
}

#endif

//-----------------------------------------------------------------------------
// Actual flash programming shims (work whether or not PICO_NO_FLASH==1)

void __no_inline_not_in_flash_func(picocart_flash_range_erase)(uint32_t flash_offs, size_t count) {
#ifdef PICO_FLASH_SIZE_BYTES
    hard_assert(flash_offs + count <= PICO_FLASH_SIZE_BYTES);
#endif
    invalid_params_if(FLASH, flash_offs & (FLASH_SECTOR_SIZE - 1));
    invalid_params_if(FLASH, count & (FLASH_SECTOR_SIZE - 1));
    
    // psram_set_cs(1);
    // picocart_flash_init_boot2_copyout();
    // psram_set_cs(2);

    printf(". ");

    // No flash accesses after this point
    // __compiler_memory_barrier();
    // program_connect_internal_flash();
    // program_flash_exit_xip();

    program_flash_range_erase(flash_offs, count, FLASH_BLOCK_SIZE, FLASH_BLOCK_ERASE_CMD);

    // program_flash_flush_cache(); // Note this is needed to remove CSn IO force as well as cache flushing

    // psram_set_cs(1);
    // picocart_flash_enable_xip_via_boot2();
    // psram_set_cs(2);
}

void __no_inline_not_in_flash_func(picocart_flash_range_program)(uint32_t flash_offs, const uint8_t *data, size_t count) {
#ifdef PICO_FLASH_SIZE_BYTES
    hard_assert(flash_offs + count <= PICO_FLASH_SIZE_BYTES);
#endif
    invalid_params_if(FLASH, flash_offs & (FLASH_PAGE_SIZE - 1));
    invalid_params_if(FLASH, count & (FLASH_PAGE_SIZE - 1));
    
    // psram_set_cs(1);
    // picocart_flash_init_boot2_copyout();
    // psram_set_cs(2);

    // __compiler_memory_barrier();

    // program_connect_internal_flash();
    // program_flash_exit_xip();

    program_flash_range_program(flash_offs, data, count);
    
    // program_flash_flush_cache(); // Note this is needed to remove CSn IO force as well as cache flushing

    // psram_set_cs(1);
    // picocart_flash_enable_xip_via_boot2();
    // psram_set_cs(2);
}

//-----------------------------------------------------------------------------
// Lower-level flash access functions

#if !PICO_NO_FLASH
// Bitbanging the chip select using IO overrides, in case RAM-resident IRQs
// are still running, and the FIFO bottoms out. (the bootrom does the same)
static void __no_inline_not_in_flash_func(picocart_flash_cs_force)(bool high) {
    // uint32_t field_val = high ?
    //     IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_HIGH :
    //     IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_LOW;
    // hw_write_masked(&ioqspi_hw->io[1].ctrl,
    //     field_val << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
    //     IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS
    // );

    if (high) {
        psram_set_cs(0);
    } else {
        psram_set_cs(2);
    }
}

void __no_inline_not_in_flash_func(picocart_flash_do_cmd)(const uint8_t *txbuf, uint8_t *rxbuf, size_t count) {
    // rom_connect_internal_flash_fn connect_internal_flash = (rom_connect_internal_flash_fn)rom_func_lookup_inline(ROM_FUNC_CONNECT_INTERNAL_FLASH);
    // rom_flash_exit_xip_fn flash_exit_xip = (rom_flash_exit_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_EXIT_XIP);
    // rom_flash_flush_cache_fn flash_flush_cache = (rom_flash_flush_cache_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_FLUSH_CACHE);
    // assert(connect_internal_flash && flash_exit_xip && flash_flush_cache);
    
    // psram_set_cs(1);
    // picocart_flash_init_boot2_copyout();
    // psram_set_cs(2);

    // __compiler_memory_barrier();
    // program_connect_internal_flash();
    // program_flash_exit_xip();

    picocart_flash_cs_force(0);
    size_t tx_remaining = count;
    size_t rx_remaining = count;
    // We may be interrupted -- don't want FIFO to overflow if we're distracted.
    const size_t max_in_flight = 16 - 2;
    while (tx_remaining || rx_remaining) {
        uint32_t flags = ssi_hw->sr;
        bool can_put = !!(flags & SSI_SR_TFNF_BITS);
        bool can_get = !!(flags & SSI_SR_RFNE_BITS);
        if (can_put && tx_remaining && rx_remaining - tx_remaining < max_in_flight) {
            ssi_hw->dr0 = *txbuf++;
            --tx_remaining;
        }
        if (can_get && rx_remaining) {
            *rxbuf++ = (uint8_t)ssi_hw->dr0;
            --rx_remaining;
        }
    }
    picocart_flash_cs_force(1);

    // program_flash_flush_cache();

    // psram_set_cs(1);
    // picocart_flash_enable_xip_via_boot2();
    // psram_set_cs(2);
}
#endif

// Use standard RUID command to get a unique identifier for the flash (and
// hence the board)

static_assert(FLASH_UNIQUE_ID_SIZE_BYTES == FLASH_RUID_DATA_BYTES, "");

void picocart_flash_get_unique_id(uint8_t *id_out) {
#if PICO_NO_FLASH
    __unused uint8_t *ignore = id_out;
    panic_unsupported();
#else
    uint8_t txbuf[FLASH_RUID_TOTAL_BYTES] = {0};
    uint8_t rxbuf[FLASH_RUID_TOTAL_BYTES] = {0};
    txbuf[0] = FLASH_RUID_CMD;
    picocart_flash_do_cmd(txbuf, rxbuf, FLASH_RUID_TOTAL_BYTES);
    for (int i = 0; i < FLASH_RUID_DATA_BYTES; i++)
        id_out[i] = rxbuf[i + 1 + FLASH_RUID_DUMMY_BYTES];
#endif
}
