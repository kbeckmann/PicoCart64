/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

uint8_t pc64_sd_wait();
DRESULT fat_disk_read_pc64(BYTE* buff, LBA_t sector, UINT count);