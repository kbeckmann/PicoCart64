#!/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2022 Konrad Beckmann

import argparse
import os

chunk_size_pot = 10
chunk_size = 1 << chunk_size_pot

def uncompressed_rom(rom_data):
    rom_data_mv = memoryview(rom_data)

    code = 'const char __attribute__((section(".n64_rom.header"))) picocart_header[16] = "picocart        ";\n'
    code += 'const uint16_t __attribute__((section(".n64_rom.mapping"))) flash_rom_mapping[] = {};\n'
    code += 'const unsigned char __attribute__((section(".n64_rom"))) rom_chunks[]['+str(chunk_size)+'] = {\n'
    for chunk in range(len(rom_data)//chunk_size):
        chunk_data = rom_data_mv[chunk*chunk_size:(chunk+1)*chunk_size]
        code += "{"
        code += ','.join([hex(c) for c in chunk_data])
        code += "},\n"
    code += '\n};\n'
    return code


def compress_rom(rom_data):
    rom_data_mv = memoryview(rom_data)

    unique_chunks = []
    unique_chunks_map = {}
    chunk_mapping = []
    chunk_idx = 0
    for chunk in range(len(rom_data_mv) // chunk_size):
        chunk_data = rom_data_mv[chunk*chunk_size:(chunk+1)*chunk_size]
        if not chunk_data in unique_chunks_map:
            unique_chunks.append(chunk_data)
            unique_chunks_map[chunk_data] = chunk_idx
            chunk_mapping.append(chunk_idx)
            chunk_idx += 1
        else:
            chunk_mapping.append(unique_chunks_map[chunk_data])

    print(f"Found {len(unique_chunks)} unique chunks")

    unique_chunks_size = len(unique_chunks) * chunk_size
    chunk_mapping_size = len(chunk_mapping) * 2
    print(f"Chunk data size: {unique_chunks_size / 1024:10.2f} kB")
    print(f"Mapping size:    {chunk_mapping_size / 1024:10.2f} kB")
    print(f"Total size:      {(unique_chunks_size + chunk_mapping_size) / 1024:10.2f} kB")

    # print(chunk_mapping)

    code = 'const char __attribute__((section(".n64_rom.header"))) picocart_header[16] = "picocartcompress";\n'

    # We copy the mapping to SRAM manually.
    code += "const uint16_t __attribute__((section(\".n64_rom.mapping\"))) flash_rom_mapping[] = {\n"
    code += ", ".join([str(i) for i in chunk_mapping])
    code += "\n};\n"
    code += "const unsigned char __attribute__((section(\".n64_rom\"))) rom_chunks[][" + str(chunk_size) + "] = {\n"
    for chunk in unique_chunks:
        code += "{"
        code += ','.join([hex(c) for c in chunk])
        code += "},\n"
    code += "};\n"
    code += "#define COMPRESSED_ROM 1\n"
    code += f"#define COMPRESSION_SHIFT_AMOUNT {chunk_size_pot}\n"
    code += f"#define COMPRESSION_MASK {chunk_size - 1}\n"

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
