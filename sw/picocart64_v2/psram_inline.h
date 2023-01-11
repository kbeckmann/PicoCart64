// /**
//  * SPDX-License-Identifier: BSD-2-Clause
//  *
//  * Copyright (c) 2022 Konrad Beckmann
//  */

// #pragma once

// #include "hardware/structs/ioqspi.h"
// #include "hardware/structs/pads_qspi.h"
// #include "hardware/structs/sio.h"
// #include "hardware/structs/ssi.h"
// #include "hardware/structs/xip_ctrl.h"

// #include "hardware/regs/pads_qspi.h"

// // TODO: For now, pins_mcu1.h or pins_mcu2.h must be included before
// //       this file.

// static inline uint8_t psram_addr_to_chip(uint32_t address)
// {
// 	return ((address >> 23) & 0x7) + 1;
// }

// //   0: Deassert all CS
// // 1-8: Assert the specific PSRAM CS (1 indexed, matches U1, U2 ... U8)
// static inline void psram_set_cs(uint8_t chip)
// {
// 	uint32_t mask = (1 << PIN_DEMUX_IE) | (1 << PIN_DEMUX_A0) | (1 << PIN_DEMUX_A1) | (1 << PIN_DEMUX_A2);
// 	uint32_t new_mask;

// 	// printf("qspi_set_cs(%d)\n", chip);

// 	if (chip >= 1 && chip <= 8) {
// 		chip--;					// convert to 0-indexed
// 		new_mask = (1 << PIN_DEMUX_IE) | (chip << PIN_DEMUX_A0);
// 	} else {
// 		// Set PIN_DEMUX_IE = 0 to pull all PSRAM CS-lines high
// 		new_mask = 0;
// 	}

// 	uint32_t old_gpio_out = sio_hw->gpio_out;
// 	sio_hw->gpio_out = (old_gpio_out & (~mask)) | new_mask;
// }
