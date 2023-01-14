/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

#include "hardware/structs/ioqspi.h"
#include "hardware/structs/pads_qspi.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/xip_ctrl.h"

#include "hardware/regs/pads_qspi.h"

// Max number of chips on the board
#define MAX_MEMORY_ARRAY_CHIP_INDEX (8)
// Capcity of the PSRAM chips soldered to the board
#define PSRAM_CHIP_CAPACITY_BYTES (8 * 1024 * 1024)
// Capcity of the nor flash chips soldered to the board
#define FLASH_CHIP_CAPACITY_BYTES (16 * 1024 * 1024)
// Starting chip index(U?) of the chip where a rom is first loaded
#define START_ROM_LOAD_CHIP_INDEX (3)
// Starting index of first flash chip used to store rom data
#define FLASH_CHIP_INDEX (7)

uint8_t psram_addr_to_chip(uint32_t address);
void psram_set_cs(uint8_t chip);

void set_demux_mcu_variables(int demux_pin0, int demux_pin1, int demux_pin2, int demux_pinIE);
void current_mcu_enable_demux(bool enabled);

bool isChipIndexFlash(uint index);