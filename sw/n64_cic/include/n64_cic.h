/**
 * SPDX-License-Identifier: MIT License
 *
 * Copyright (c) 2019 Jan Goldacker
 * Copyright (c) 2021-2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Callback will be called when the CIC receives a reset command.
// This happens when the users presses the physical Reset button.
typedef void (*cic_reset_cb_t)(void);

void n64_cic_reset_parameters(void);
void n64_cic_set_parameters(uint32_t * args);
void n64_cic_set_dd_mode(bool enabled);
void n64_cic_hw_init(void);
void n64_cic_task(cic_reset_cb_t cic_reset_cb);
