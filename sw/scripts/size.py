#!/bin/env python3

import argparse
import sys
import os
import subprocess
import re
from elftools.elf.elffile import ELFFile

def print_size(args):
    f = open(args.elf, "rb")
    elffile = ELFFile(f)

    text = elffile.get_section_by_name('.text')
    for x in text:
        print(x)

    print(text)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Prints usage")
    parser.add_argument(
        "--elf",
        type=str,
        default="build/gw_retro_go.elf",
        help="Game and Watch Retro-Go ELF file",
    )

    print_size(parser.parse_args())

