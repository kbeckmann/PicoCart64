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

void dump_current_ssi_config();

void qspi_print_pull(void);
void qspi_set_pull(bool disabled, bool pullup, bool pulldown);
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

void __no_inline_not_in_flash_func(flash_bulk_read)(uint32_t *rxbuf, uint32_t flash_offs, size_t len, uint dma_chan);
