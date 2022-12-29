/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

void send_address(uint32_t address);
uint32_t read32();
uint16_t read16();
void verify_data(uint32_t data, uint32_t address);