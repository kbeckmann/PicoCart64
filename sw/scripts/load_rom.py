#!/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2022 Konrad Beckmann

import argparse
import os

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Load .z64 rom into rom.h')
    parser.add_argument('rom', type=str)

    args = parser.parse_args()

    rom_h = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', 'picocart64', 'rom.h')

    with open(args.rom, 'rb') as r:
        with open(rom_h, 'w') as f:
            f.write('const unsigned char __in_flash("rom_file") rom_file[] = {\n')
            f.write(','.join([hex(c) for c in r.read()]))
            f.write('\n};\n')

    print(f'Wrote rom contents to {rom_h}')
