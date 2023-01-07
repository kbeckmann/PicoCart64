#!/bin/bash
# NOTE : Quote it else use array to avoid problems #
FILES="/Volumes/UNTITLED/N64/boxart/*.png"
for f in $FILES
do
	filename=$(basename -- "$f")
	extension="${filename##*.}"
	filename="${filename%.*}"
	spritefilename="${filename}.sprite"
	echo "Converting ${f} to ${spritefilename}"
	~/Code/n64/libdragon/libdragon/tools/mksprite/mksprite 16 "$f" "./dump/${spritefilename}"
done