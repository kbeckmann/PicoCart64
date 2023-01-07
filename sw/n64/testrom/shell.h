/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

void		start_shell(void);

int load_boxart_for_rom(char* filename);
int read_rom_header_serial_number(char* buf, const char* const filename);
