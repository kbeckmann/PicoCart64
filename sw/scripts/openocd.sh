#!/bin/bash
set -e

OPENOCD=${OPENOCD:-$(which openocd || true)}

# Hacky but apparently reliable way to get the absolute path of the script directory
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BUILDPATH=${BUILDPATH:-$SCRIPTPATH/../build}
ELF="$BUILDPATH/picocart64_v2/picocart64_v2.elf"

# Set sio_hw->gpio_out to 0 in order to turn off MCU1
# &sio_hw->gpio_out 0xd0000010
# sio_hw->gpio_out = 0

CMD="init; halt; mww 0xd0000010 0; $@"

$OPENOCD \
    -f interface/jlink.cfg \
    -c "transport select swd; adapter_khz 1000" \
    -f target/rp2040.cfg \
    -c "$CMD"

