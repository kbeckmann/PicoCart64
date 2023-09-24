/**
 * SPDX-License-Identifier: MIT License
 *
 * Copyright (c) 2019 Jan Goldacker
 * Copyright (c) 2021-2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 */

#pragma once

// Callback will be called when the CIC sends a reset command.
// This happens when the users presses the physical Reset button.
typedef void (*cic_reset_cb_t)(void);

void n64_cic_run(cic_reset_cb_t cic_reset_cb);
