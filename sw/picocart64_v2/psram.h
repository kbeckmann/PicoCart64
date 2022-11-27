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

// extern int current_mcu_demux_pin_0;
// extern int current_mcu_demux_pin_1;
// extern int current_mcu_demux_pin_2;
// extern int current_mcu_demux_pin_ie;

uint8_t psram_addr_to_chip(uint32_t address);
void psram_set_cs(uint8_t chip);

void set_demux_mcu_variables(int demux_pin0, int demux_pin1, int demux_pin2, int demux_pinIE);
void current_mcu_enable_demux(bool enabled);