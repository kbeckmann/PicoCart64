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

#include "hardware/regs/pads_qspi.h"

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

void qspi_print_pull(void);
void qspi_set_pull(bool disabled, bool pullup, bool pulldown);
void qspi_oeover_normal(bool enable_ss);
void qspi_oeover_disable(void);

void qspi_demux_enable(bool enabled);

void qspi_enter_cmd_xip(void);
void qspi_init_spi(void);
void qspi_enable(void);
void qspi_disable(void);
void qspi_write(uint32_t address, const uint8_t * data, uint32_t length);
void qspi_read(uint32_t address, uint8_t * data, uint32_t length);

void qspi_test(void);
