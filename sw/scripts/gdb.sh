#!/bin/bash
set -e

OPENOCD=${OPENOCD:-$(which openocd || true)}
PREFIX=${PREFIX:-"arm-none-eabi-"}

# Hacky but apparently reliable way to get the absolute path of the script directory
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BUILDPATH=${BUILDPATH:-$SCRIPTPATH/../build}
ELF="$BUILDPATH/picocart64_v2/picocart64_v2.elf"
GDB=${PREFIX}gdb

$GDB $ELF -ex 'target extended-remote :3333'
