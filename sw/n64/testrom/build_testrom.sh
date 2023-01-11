#!/bin/sh

is_rebuild=false
build_firmware=false

for param in "$@"
do
	if [[ "$param" == "rebuild" ]]
	then
		is_rebuild=true
	fi
	
	if [[ "$param" == "firmware" ]]
	then
		build_firmware=true
	fi
done

echo "\n\nBuilding ROM: is_rebuild: ${is_rebuild}, build_firmware: ${build_firmware}..."

if [ "$is_rebuild" = true ]
then
	echo "Rebuilding libdragon..."
	sudo libdragon install
	echo "DONE"
	
	echo "Cleaning build cache..."
	# also clean the rom build cache to get a clean build
	sudo libdragon make clean
fi

echo "Building rom..."
sudo libdragon make

echo "\n\nExecuting ROM Script on ROM..."
python3 /Users/kaili/Code/PicoCart64/sw/scripts/load_rom.py --compress /Users/kaili/Code/PicoCart64/sw/n64/testrom/build/testrom.z64

echo "\n\nCopying Header file to duplicate location..."
cp -v /Users/kaili/Code/PicoCart64/sw/generated/rom.h /Users/kaili/Code/PicoCart64/sw/picocart64/rom.h

if [ "$build_firmware" = true ]
then
	echo "Building firmware..."
	(cd /Users/kaili/Code/PicoCart64/sw/build/ && make)
fi

echo "\n\nDONE!\n\n"