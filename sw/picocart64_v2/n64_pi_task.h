/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

enum {
    CORE1_SEND_SD_READ_CMD,
    CORE1_UPDATE_ROM_CACHE,
    CORE1_LOAD_NEW_ROM_CMD
};
void n64_pi_run(void);
void restart_n64_pi_pio();
extern volatile bool g_restart_pi_handler;


