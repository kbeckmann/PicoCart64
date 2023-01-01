/**
 * Kaili Hill 2022
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copy of the pico program_flash_generic.h file
 */

#ifndef _PROGRAM_FLASH_ARRAY_H
#define _PROGRAM_FLASH_ARRAY_H

#include <stdint.h>
#include <stddef.h>

void program_connect_internal_flash();
void program_flash_init_spi();
void program_flash_put_get(const uint8_t *tx, uint8_t *rx, size_t count, size_t rx_skip);
void program_flash_do_cmd(uint8_t cmd, const uint8_t *tx, uint8_t *rx, size_t count);
void program_flash_exit_xip();
void program_flash_page_program(uint32_t addr, const uint8_t *data);
void program_flash_range_program(uint32_t addr, const uint8_t *data, size_t count);
void program_flash_sector_erase(uint32_t addr);
void program_flash_user_erase(uint32_t addr, uint8_t cmd);
void program_flash_range_erase(uint32_t addr, size_t count, uint32_t block_size, uint8_t block_cmd);
void program_flash_read_data(uint32_t addr, uint8_t *rx, size_t count);
int  program_flash_size_log2();
void program_flash_flush_cache();
void program_flash_enter_cmd_xip();
void program_flash_abort();
int  program_flash_was_aborted();

void program_flash_disable();
io_rw_32 grab_ctrlr0();
io_rw_32 grab_spi_ctrlr_0();

void program_write_buf(uint32_t addr, const uint8_t* data, uint32_t len);

#endif // _PROGRAM_FLASH_ARRAY_H