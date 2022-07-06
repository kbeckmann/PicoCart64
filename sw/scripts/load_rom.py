#!/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2022 Konrad Beckmann

import argparse
import os


def uncompressed_rom(rom_data):
    code = 'const unsigned char __in_flash("rom_file") rom_file[] = {\n'
    code += ','.join([hex(c) for c in rom_data])
    code += '\n};\n'
    return code




def compress_rom(rom_data):
    chunk_size = 4096

    assert(len(rom_data) % chunk_size == 0)

    rom_data_ba = bytearray(rom_data)

    unique_chunks = []
    chunk_mapping = []
    for chunk in range(len(rom_data_ba) // chunk_size):
        chunk_data = rom_data_ba[chunk*chunk_size:(chunk+1)*chunk_size]
        if not chunk_data in unique_chunks:
            unique_chunks.append(chunk_data)
        chunk_mapping.append(unique_chunks.index(chunk_data))
    print(f"Found {len(unique_chunks)} unique chunks")
    # print(chunk_mapping)

    # Note the lack of __in_flash() - we want the mapping to be copied to SRAM.
    code = "const uint16_t rom_mapping[] = {\n"
    code += ", ".join([str(i) for i in chunk_mapping])
    code += "\n};"
    code += "const unsigned char __in_flash(\"rom_file\") rom_chunks[][" + str(chunk_size) + "] = {\n"
    for chunk in unique_chunks:
        code += "{"
        code += ','.join([hex(c) for c in chunk])
        code += "},"
    code += "};\n"
    code += "#define COMPRESSED_ROM 1"

    return code





if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Load .z64 rom into rom.h')
    parser.add_argument(
        "--compress",
        dest="compress",
        action="store_true",
        help="Enable experimental compression",
    )
    parser.add_argument('rom', type=str)

    args = parser.parse_args()

    rom_h = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', 'picocart64', 'rom.h')

    with open(args.rom, 'rb') as r:
        rom_data = r.read()
        if args.compress:
            code = compress_rom(rom_data)
        else:
            code = uncompressed_rom(rom_data)
        with open(rom_h, 'w') as f:
            f.write(code)
        print(f'Wrote rom contents to {rom_h}')
