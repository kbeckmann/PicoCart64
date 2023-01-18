# DREAMDrive64
This is a fork of the original PicoCart64. Sales coming soon at dreamcraftindustries.com. Stay tuned!

In development Nintendo 64 flash cart based around Raspberry Pi RP2040s.

# Current Dev Status
* Development is in progress with firmware at a functional stage although missing some features.
* v1-lite versions can load roms up to 16MBs
  * Are basic boards useful for developers wanting to test on real hardware.
  * Barebones, easy to build yourself.
* v2 based boards (DREAMDrive64) can boot roms with active firmware development in progress. 
	* SD read speed is currently limited by the SPI interface. Looking into SDIO upgrades for faster rom load times
	* Menu rom can navigate the sd file system assuming a rational number of files/folders (256 at the moment per directory, 10 directory depth limit)
	* Menu rom can render thumbnail boxart of a rom when scrolling through file list
	* Libdragon changes are needed if you want to compile your own rom for the firmware, those changes aren't up anywhere... yet!
  * Hardware design is still in flux. Only attempt to build your own if you are willing to debug random hardware/software issues.

## Features
| Feature					| Status 	 | Notes 	|
|--------------------------:|:-----------|---------:|
| Rom Loading				| Complete!	 |			|
| SD Filesystem navigation	| Partial	 |  		|
| Wifi						| Soon		 |			|
| EEPROM save support		| Partial		 |			|
| SRAM support 				| Complete!  |			|

**ROM compatibility is still WIP**

# Join in on the fun!
Dreamcraft Industries discord

[![Join the Discord!](https://discordapp.com/api/guilds/989902502063398982/widget.png?style=banner3)](https://discord.gg/yawWMcv6tC)

Original PicoCart64 discord
Join the Discord server to participate in the discussion and development of this project.
[!Dubious Technology Discord](https://discord.gg/2Gb3jWqqja)