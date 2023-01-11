/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <libdragon.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rom_utils.h"

int rom_extract_header(char* filename, struct rom_header_t* header) {
    FILE* fp = fopen(filename, "r");

    if (!fp) {
        fclose(fp);
        return -1;
    }

    //uint8_t* b = malloc(64);    
    uint numRead = fread((uint8_t*)header, 1, sizeof(rom_header_t), fp);
    if (numRead == 0) {
        fclose(fp);
        return -2;
    }
    fclose(fp);

    // janky method of checking for 80 37 12 40 bytes
    // This will only be true for .z64 big-endian roms
    if (!rom_is_valid_n64_rom((uint8_t*)header)) {
        return -3;
    }

    return 0;
}

bool rom_is_valid_n64_rom(uint8_t* buf) {
    return (buf[0] != 0x80 && buf[1] != 0x37 && buf[2] != 0x12 && buf[3] != 0x40);
}

void rom_country_code(uint8_t code, char* buf) {
    switch (code) {
    case 0x37: buf = "Beta"; break;
    case 0x41: buf = "Asian (NTSC)"; break;
    case 0x42: buf = "Brazilian"; break;
    case 0x43: buf = "Chinese"; break;
    case 0x44: buf = "German"; break;
    case 0x45: buf = "North America"; break;
    case 0x46: buf = "French"; break;
    case 0x47: buf = "Gateway 64 (NTSC)"; break;
    case 0x48: buf = "Dutch"; break;
    case 0x49: buf = "Italian"; break;
    case 0x4A: buf = "Japanese"; break;
    case 0x4B: buf = "Korean"; break;
    case 0x4C: buf = "Gateway 64 (PAL)"; break;
    case 0x4E: buf = "Canadian"; break;
    case 0x50: buf = "European (basic spec.)"; break;
    case 0x53: buf = "Spanish"; break;
    case 0x55: buf = "Australian"; break;
    case 0x57: buf = "Scandinavian"; break;
    case 0x58: buf = "European"; break;
    case 0x59: buf = "European"; break;
    }
}
