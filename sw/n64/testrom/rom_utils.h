/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#pragma once

/*
Offset|Bytes|Notes
0x00	x	indicator for endianness (nybble)*
0x01	x	initial PI_BSB_DOM1_LAT_REG (nybble)*
0x01	x	initial PI_BSD_DOM1_PGS_REG (nybble)*
0x02	1	initial PI_BSD_DOM1_PWD_REG*
0x03	1	initial PI_BSB_DOM1_PGS_REG*
0x04	4	ClockRate Override(0 uses default)*
0x08	4	Program Counter*
0x0C	4	Release Address
0x10	4	CRC1 (checksum)
0x14	4	CRC2
0x18	8	Unknown/Not used
0x20	20	Image Name/Internal name*
0x34	4	Unknown/Not used
0x38	4	Media format
0x3C	2	Cartridge ID (alphanumeric)
0x3E	1	Country Code
0x3F	1	Version
First 8 bytes
80 37 12 40 03 A0 7F 5F
*/
typedef struct rom_header_t {
    uint8_t endianness; 
    uint8_t pulse_page[2];
    uint8_t bus_speed[2];
    uint8_t unused0[8];
    uint8_t crc1[4];
    uint8_t crc2[4];
    uint8_t notused0[8];
    uint8_t name[20];
    uint8_t notused1[4];
    uint8_t media_format[4];
    uint8_t cartridge_id[4];
    uint8_t country_code;
    uint8_t version;
} rom_header_t;

/*
 * Filename(including path) to file whos header should be extracted
 * return status code
 *  0 = success
 * -1 = cannot open file
 * -2 = read failed
 * -3 = not a valid n64 rom
 */
int rom_extract_header(char* filename, struct rom_header_t* header);

/*
 * Pass in at least the first 4 bytes of a file
 */
bool rom_is_valid_n64_rom(uint8_t* buf);

/*
 * Given a country code, return a string with the country
 */
void rom_country_code(uint8_t code, char* ret);
