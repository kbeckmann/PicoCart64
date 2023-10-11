# PicoCart64
Nintendo 64 flash cart using a Raspberry Pi Pico / RP2040. So far, all of the work happens in the `develop` branch and is highly experimental. Follow kbeckmann on [twitter](https://twitter.com/kbeckmann) to follow along while he's working on the project.

Join the Discord server to participate in the discussion and development of this project.

[![Join the Discord!](https://discordapp.com/api/guilds/989902502063398982/widget.png?style=banner3)](https://discord.gg/CGTjxkVr7P)

There are a few versions:
- PicoCart64 v1 lite - The _lite_ version using a Raspberry Pi Pico board capable of booting homebrew. This is the most user friendly option as of now.
- PicoCart64 v1 - The _stamp_ version using a RP2040 Stamp from Solder Party. This is used as an experimental development platform for the _next-gen_ version.
- PicoCart64 v2 - The _next-gen_ version which has two RP2040 chips, on-board PSRAM chip supporting ROMs up to 64MB, a WiFi module and SD-card support. This is still heavy in development. Don't build this unless you want to develop it.

The following concerns PicoCart64 v1 lite, unless noted.

# How to build a PicoCart64 v1 lite

## Getting the raw PCB

Check out the `develop` branch and navigate to `hw/picocart64_v1_lite/`. Compress the `gerb_picocart64-lite_v1_3` to a `.zip` file.

Go to a PCB manufacturing service such as [JLCPCB](https://jlcpcb.com), [OSH Park](https://oshpark.com/) or similar.

This takes you to the PCB configuration utility where you can upload the `.zip` file you just created. After uploading, you should see a rendered version of both sides of the PCB. If you want to produce a cheap cart for testing you can leave all settings as they are, except for this one:

:exclamation: *PCB Thickness* needs to be set to *1.2mm*, otherwise the cart won't fit in your console

:exclamation: Note that this will result in a PCB with 90 degrees edges - the edge connector should be sanded down to get a nice 45 degree V shape. If this is not done, it may wear down your cart connector on the N64. The proper PCB order should be ENIG-RoHS + Gold Fingers + 45°finger chamfered, however this will result in a significant cost increase.

## Sourcing the components

To build the v1 lite version, you need the following parts along with the PCBs. Here we're assuming you want to assemble 5 boards, as that is the number of PCBs you'll be getting from JLCPCB as a minimum.

Note that the Raspberry Pi Pico only has 2MB of flash, which limits the size of ROMs that can be used.

If you need more flash, you can order a Pico-compatible board with more flash. WeAct RP2040 16 MB can be bought from [Aliexpress](https://www.aliexpress.com/item/1005003708090298.html). Note that you need to bridge the solder bridge JP1 on the PicoCart 64 v1 lite board if using this.

| Qty | Name                  | Source                                                                           | 
|-----|-----------------------|----------------------------------------------------------------------------------|
| 2   | Raspberry Pi Pico 3-pack | [713-102110545](https://www.mouser.com/ProductDetail/713-102110545)               |       |
| 5   | BSS84 MOSFET          | [750-BSS84-HF](https://www.mouser.com/ProductDetail/750-BSS84-HF)                 |       |
| 5   | 0603 100k resistor    | [71-CRCW0603100KJNEAC](https://www.mouser.com/ProductDetail/71-CRCW0603100KJNEAC) |       |

## Assembling the board
Line up the Pico with the USB connector facing outwards and solder the pads.

Solder the MOSFET according to the pin markings.

The resistor is optional to solder, but was placed there for good measure. (There is a resistor on the Pico, so it is not strictly needed.)

## Uploading firmware and a ROM

[This website](https://kbeckmann.github.io/PicoCart64/) guides you through the steps to program the required firmware, as well as how to generate and program a ROM file to the PicoCart64 v1 lite.

Please note that this tool can generate programming files that are larger than 2MB, and will not fit in that case on the original Raspberry Pi Pico 2MB.

## License

The PCB is licensed under the following license: "CERN Open Hardware Licence Version 2 - Permissive" aka "CERN-OHL-P".

The software is licensed under BSD-2-Clause unless otherwise specified.
