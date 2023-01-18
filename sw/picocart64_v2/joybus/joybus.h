#pragma once


#include <stdbool.h>
#include <stdint.h>

#include <hardware/pio.h>

#define JOYBUS_BUFFER_SIZE      (64)

enum joybus_cmd_e {
    JOYBUS_CMD_INFO = 0x00,
    JOYBUS_CMD_STATE = 0x01,
    JOYBUS_CMD_READ = 0x02,
    JOYBUS_CMD_WRITE = 0x03,
    JOYBUS_CMD_EEPROM_READ = 0x04,
    JOYBUS_CMD_EEPROM_WRITE = 0x05,
    JOYBUS_CMD_RESET = 0xFF,
};

#define EEPROM_TYPE_4K (0x0080)
#define EEPROM_TYPE_16K (0x00C0)
#define EEPROM_WRITE_IN_PROGRESS (0x80)

extern volatile uint8_t eeprom[];
extern volatile uint16_t eeprom_type;

void enable_joybus();
void disable_joybus();
