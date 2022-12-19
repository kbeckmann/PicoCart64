/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

void send_address(uint32_t address);
uint16_t start_read();
void verify_data(uint16_t data, uint32_t address);