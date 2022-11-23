/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include "hardware/structs/ioqspi.h"
#include "hardware/structs/pads_qspi.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/dma.h"
#include "hardware/regs/pads_qspi.h"
#include "hardware/regs/io_qspi.h"

#include "gpio_helper.h"

#define QSPI_SCLK_PIN  (0)
#define QSPI_SS_PIN    (1)
#define QSPI_SD0_PIN   (2)
#define QSPI_SD1_PIN   (3)
#define QSPI_SD2_PIN   (4)
#define QSPI_SD3_PIN   (5)

// It seems the PAD order is different
#define QSPI_SCLK_PAD  (0)
#define QSPI_SD0_PAD   (1)
#define QSPI_SD1_PAD   (2)
#define QSPI_SD2_PAD   (3)
#define QSPI_SD3_PAD   (4)
#define QSPI_SS_PAD    (5)

#define DEBUG_CS_CHIP_USE 2

extern uint32_t log_buffer[128]; // store addresses
void add_log_to_buffer(uint32_t value);

extern volatile uint32_t update_rom_cache_for_address;
void load_rom_cache(uint32_t startingAt);
void update_rom_cache(uint32_t address);

void dump_current_ssi_config();

void qspi_print_pull(void);
void qspi_set_pull(bool disabled, bool pullup, bool pulldown);
void qspi_restore_to_startup_config();
void qspi_oeover_normal(bool enable_ss);
void qspi_oeover_disable(void);

void qspi_demux_enable(bool enabled);

void qspi_enter_cmd_xip(void);
void qspi_enter_cmd_xip_with_params(uint8_t cmd, bool quad_addr, bool quad_data, int dfs, int addr_l, int waitCycles, int baudDivider);
void qspi_init_spi(void);
void qspi_init_qspi(void);
void qspi_enable();
void qspi_disable();
void qspi_write(uint32_t address, const uint8_t * data, uint32_t length);
void qspi_read(uint32_t address, uint8_t * data, uint32_t length);

void qspi_test(void);

// FLASH FUNCTIONS
// void qspi_flash_init();
// void qspi_flash_init_spi(); // used before issuing serial commands to flash (ie erase/write)

// void qspi_flash_enable_xip();
// void qspi_flash_enable_xip2();
// void __no_inline_not_in_flash_func(qspi_flash_bulk_read)(uint32_t cmd, uint32_t *rxbuf, uint32_t flash_offs, size_t len, uint dma_chan);
// void qspi_flash_write(uint32_t address, uint8_t * data, uint32_t length);
// void qspi_flash_erase_block(uint32_t address);
// void qspi_flash_read_data(uint32_t addr, uint8_t *rx, size_t count);

// // INTERNAL, don't use these unless you know what you're doing
// void __noinline qspi_flash_put_get(const uint8_t *tx, uint8_t *rx, size_t count, size_t rx_skip);
// void qspi_flash_do_cmd(uint8_t chip, uint8_t cmd, const uint8_t *tx, uint8_t *rx, size_t count);