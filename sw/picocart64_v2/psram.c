#include "stdio.h"
#include "hardware/gpio.h"

#include "utils.h"
#include "psram.h"
#include "gpio_helper.h"

/*
U1 = FLASH, BOOT
U2 = Unoccupied
U3 = PSRAM 8M
U4 = PSRAM 8M
U5 = PSRAM 8M
U6 = PSRAM 8M
U7 = FLASH 16M
U8 = FLASH 16M
*/ 

const int MAX_MEMORY_ARRAY_CHIP_INDEX = 8;
const uint32_t PSRAM_CHIP_CAPACITY_BYTES = 8 * 1024 * 1024;
const uint32_t FLASH_CHIP_CAPACITY_BYTES = 16 * 1024 * 1024;
const int START_ROM_LOAD_CHIP_INDEX = 3;

volatile int current_mcu_demux_pin_0 =  -1;
volatile int current_mcu_demux_pin_1 =  -1;
volatile int current_mcu_demux_pin_2 =  -1;
volatile int current_mcu_demux_pin_ie = -1;

static gpio_config_t current_demux_disabled_config[3];
static gpio_config_t current_demux_enabled_config[3];

void set_demux_mcu_variables(int demux_pin0, int demux_pin1, int demux_pin2, int demux_pinIE) {
    current_mcu_demux_pin_0 = demux_pin0;
    current_mcu_demux_pin_1 = demux_pin1;
    current_mcu_demux_pin_2 = demux_pin2;
    current_mcu_demux_pin_ie = demux_pinIE;

    current_demux_disabled_config[0] = (gpio_config_t){demux_pin0, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};   
    current_demux_disabled_config[1] = (gpio_config_t){demux_pin1, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};
    current_demux_disabled_config[2] = (gpio_config_t){demux_pin2, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};
    current_demux_disabled_config[3] = (gpio_config_t){demux_pinIE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};

    current_demux_enabled_config[0] = (gpio_config_t){demux_pin0, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};   
    current_demux_enabled_config[1] = (gpio_config_t){demux_pin1, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};
    current_demux_enabled_config[2] = (gpio_config_t){demux_pin2, GPIO_OUT, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};
    current_demux_enabled_config[3] = (gpio_config_t){demux_pinIE, GPIO_IN, false, false, false, GPIO_DRIVE_STRENGTH_4MA, GPIO_FUNC_SIO};
}

inline uint8_t psram_addr_to_chip(uint32_t address)
{
	// return ((address >> 23) & 0x7) + 1;
	return ((address >> 23) & 0x7) + 3; // +3 since psram is soldered starting at 3 on my test board
}

//   0: Deassert all CS
// 1-8: Assert the specific PSRAM CS (1 indexed, matches U1, U2 ... U8)
void psram_set_cs(uint8_t chip)
{
	uint32_t mask = (1 << current_mcu_demux_pin_ie) | (1 << current_mcu_demux_pin_0) | (1 << current_mcu_demux_pin_1) | (1 << current_mcu_demux_pin_2);
	uint32_t new_mask;

	// printf("qspi_set_cs(%d)\n", chip);

	if (chip >= 1 && chip <= 8) {
		chip--;					// convert to 0-indexed
		new_mask = (1 << current_mcu_demux_pin_ie) | (chip << current_mcu_demux_pin_0);
	} else {
		// Set PIN_DEMUX_IE = 0 to pull all PSRAM CS-lines high
		new_mask = 0;
	}

	uint32_t old_gpio_out = sio_hw->gpio_out;
	sio_hw->gpio_out = (old_gpio_out & (~mask)) | new_mask;
}

void current_mcu_enable_demux(bool enabled) {
	if (enabled) {
		gpio_configure(current_demux_enabled_config, ARRAY_SIZE(current_demux_enabled_config));
	} else {
		gpio_configure(current_demux_disabled_config, ARRAY_SIZE(current_demux_disabled_config));
	}
}
