/**
 * SPDX-License-Identifier: MIT License
 *
 * Copyright (c) 2019 Jan Goldacker
 * Copyright (c) 2021-2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback function type for CIC reset
 * 
 * This callback will be called when the CIC receives a reset command.
 * This happens when the user presses the physical Reset button.
 */
typedef void (*cic_reset_cb_t)(void);

/**
 * @brief Reset the parameters of the N64 CIC
 */
void n64_cic_reset_parameters(void);

/**
 * @brief Set the parameters of the N64 CIC
 * 
 * @param args The parameters to set
 */
void n64_cic_set_parameters(uint32_t * args);

/**
 * @brief Enable or disable DD mode in the N64 CIC
 * 
 * @param enabled Whether to enable or disable DD mode
 */
void n64_cic_set_dd_mode(bool enabled);

/**
 * @brief Initialize the N64 CIC hardware
 */
void n64_cic_hw_init(void);

/**
 * @brief Run the N64 CIC task
 * 
 * @param cic_reset_cb The callback to call when the CIC receives a reset command
 */
void n64_cic_task(cic_reset_cb_t cic_reset_cb);
