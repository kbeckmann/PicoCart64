#pragma once

#include "hardware/regs/addressmap.h"
#include "hardware/regs/vreg_and_chip_reset.h"

static inline io_rw_32 get_reset_reason(void)
{
	return *((io_rw_32 *) (VREG_AND_CHIP_RESET_BASE + VREG_AND_CHIP_RESET_CHIP_RESET_OFFSET));
}
