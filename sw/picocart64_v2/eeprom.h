/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

extern volatile uint8_t process_joybus_buf;

void init_joybus();
void read_joybus();

void read_eeprom(uint8_t block);
void write_eeprom(uint8_t block, uint8_t* buf);

