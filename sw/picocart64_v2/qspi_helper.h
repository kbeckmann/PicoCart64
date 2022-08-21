/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

#define QSPI_SCLK_PIN  (0)
#define QSPI_SS_PIN    (1)
#define QSPI_SD0_PIN   (2)
#define QSPI_SD1_PIN   (3)
#define QSPI_SD2_PIN   (4)
#define QSPI_SD3_PIN   (5)

static void inline qspi_oeover_normal(bool ss)
{
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	if (ss) {
		ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
										   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	} else {
		ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
										   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));
	}

	ioqspi_hw->io[QSPI_SD0_PIN].ctrl = (ioqspi_hw->io[QSPI_SD1_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD1_PIN].ctrl = (ioqspi_hw->io[QSPI_SD2_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD2_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD3_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_VALUE_NORMAL << IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_LSB));
}

static void inline qspi_oeover_disable(void)
{
	ioqspi_hw->io[QSPI_SCLK_PIN].ctrl = (ioqspi_hw->io[QSPI_SCLK_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_BITS) |
										 (IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SS_PIN].ctrl = (ioqspi_hw->io[QSPI_SS_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS) |
									   (IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD0_PIN].ctrl = (ioqspi_hw->io[QSPI_SD1_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD0_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD1_PIN].ctrl = (ioqspi_hw->io[QSPI_SD2_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD2_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD2_CTRL_OEOVER_LSB));

	ioqspi_hw->io[QSPI_SD3_PIN].ctrl = (ioqspi_hw->io[QSPI_SD3_PIN].ctrl & (~IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_BITS) |
										(IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_VALUE_DISABLE << IO_QSPI_GPIO_QSPI_SD3_CTRL_OEOVER_LSB));
}
