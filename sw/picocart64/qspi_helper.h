#pragma once

#include <stdint.h>

#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/xip_ctrl.h"

#define QSPI_SCLK_PIN  (0)
#define QSPI_SS_PIN    (1)
#define QSPI_SD0_PIN   (2)
#define QSPI_SD1_PIN   (3)
#define QSPI_SD2_PIN   (4)
#define QSPI_SD3_PIN   (5)

#define QSPI_SS_GPIO_PIN_MASK (UINT32_MAX)

#define QSPI_GPIO_XIP_NORMAL ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_XIP_SCLK << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_INPUT ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_DISABLE   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_HIGH     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_OUTPUT_HIGH ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_ENABLE    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_HIGH     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_OUTPUT_LOW ( \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_VALUE_NORMAL   << IO_QSPI_GPIO_QSPI_SCLK_CTRL_IRQOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_VALUE_NORMAL    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_INOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_VALUE_ENABLE    << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OEOVER_LSB)  | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_VALUE_LOW      << IO_QSPI_GPIO_QSPI_SCLK_CTRL_OUTOVER_LSB) | \
		(IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_VALUE_NULL     << IO_QSPI_GPIO_QSPI_SCLK_CTRL_FUNCSEL_LSB)   \
)

#define QSPI_GPIO_INPUT_GET(_pin) (ioqspi_hw->io[(_pin)].status & IO_QSPI_GPIO_QSPI_SCLK_STATUS_INFROMPAD_BITS)

static inline uint32_t qspi_read_u32(const uint32_t * addr, uint32_t ss_gpio_pin_mask)
{
	io_rw_32 *ss_ctrl = &ioqspi_hw->io[QSPI_SS_PIN].ctrl;

	uint32_t data_u32;

	if (ss_gpio_pin_mask == QSPI_SS_GPIO_PIN_MASK) {
		// Control the dedicated QSPI CS pin manually

		// CSn = 0
		*ss_ctrl = QSPI_GPIO_OUTPUT_LOW;

		data_u32 = *addr;

		// CSn = 1
		*ss_ctrl = QSPI_GPIO_OUTPUT_HIGH;
	} else {
		// The provided cs_pin is a GPIO

		// CSn = 0
		gpio_clr_mask(ss_gpio_pin_mask);

		data_u32 = *addr;

		// CSn = 1
		gpio_set_mask(ss_gpio_pin_mask);
	}

	return data_u32;
}

static inline uint16_t qspi_read_u16(const uint16_t * addr, uint32_t ss_gpio_pin_mask)
{
	io_rw_32 *ss_ctrl = &ioqspi_hw->io[QSPI_SS_PIN].ctrl;

	uint32_t data_u16;

	if (ss_gpio_pin_mask == QSPI_SS_GPIO_PIN_MASK) {
		// Control the dedicated QSPI CS pin manually

		// SSn = 0
		*ss_ctrl = QSPI_GPIO_OUTPUT_LOW;

		data_u16 = *addr;

		// SSn = 1
		*ss_ctrl = QSPI_GPIO_OUTPUT_HIGH;
	} else {
		// The provided cs_pin is a GPIO

		// SSn = 0
		gpio_clr_mask(ss_gpio_pin_mask);

		data_u16 = *addr;

		// SSn = 1
		gpio_set_mask(ss_gpio_pin_mask);
	}

	return data_u16;
}
