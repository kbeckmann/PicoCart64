#!/bin/sh

echo "\n\nBuilding ROM..."
sudo libdragon make

echo "\n\nExecuting ROM Script on ROM..."
python3 /Users/kaili/Code/PicoCart64/sw/scripts/load_rom.py --compress /Users/kaili/Code/PicoCart64/sw/n64/testrom/build/testrom.z64

echo "\n\nCopying Header file to duplicate location..."
cp -v /Users/kaili/Code/PicoCart64/sw/generated/rom.h /Users/kaili/Code/PicoCart64/sw/picocart64/rom.h

echo "\n\nDONE!\n\n"