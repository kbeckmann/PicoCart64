# PicoCart64
Nintendo 64 flash cart using a Raspberry Pi Pico. So far, all of the work happens in the `develop` branch. Follow [kbeckmann](https://twitch.tv/kbeckmann) to follow along while he's working on the project. Konrad also hosts a [Discord server](https://discord.com/invite/Q6HaRzKm) where there's also a community budding.

There are parallel projects currently:
- The _lite_ version using one Raspberry Pi Pico board capable of booting homebrew
- The _next-gen_ version with two RP2040 chips and an on-board PSRAM chip supporting ROMs up to 64MB.

The following concerns the _lite_ project, unless noted.

# Getting up to speed
Although PicoCart64 is still a very young project there are several people interested in contributing already. A PicoCart64-lite board is capable of loading up to 2MB ROMs on an unmodified N64. 

## Getting the raw PCB
_Note that currently this is a work-in-progress, and even the first version of the PCB has not been tested._

Check out the `develop` branch and navigate to `hw/picocart64_v1_lite/`. Compress the `gerb_picocart64-lite_v1_0` to a `.zip` file.

Go to [JLCPCB](https://jlcpcb.com) and make an account if you don't have one already. Push the *Order now* button in the top-right.

This takes you to the PCB configuration utility where you can upload the `.zip` file you just created. After uploading, you should see a rendered version of both sides of the PCB. If you want to produce a cheap cart for testing you can leave all settings as they are, except for this one:

- *PCB Thickness* needs to be set to *1.0mm*, otherwise the cart won't fit in your console

## Sourcing the components
For building the lite version, you need the following parts along with the PCBs. Here we're assuming you want to assemble 5 boards, as that is the number of PCBs you'll be getting from JLCPCB as a minimum.

| Qty | Name                  | Source                                                                           | Notes |
|-----|-----------------------|----------------------------------------------------------------------------------|-------|
| 2   | Raspberry Pico 3-pack | [713-102110545](https://www.mouser.dk/ProductDetail/713-102110545)               |       |
| 5   | BSS84 MOSFET          | [750-BSS84-HF](https://www.mouser.dk/ProductDetail/750-BSS84-HF)                 |       |
| 5   | 0603 100k resistor    | [71-CRCW0603100KJNEAC](https://www.mouser.dk/ProductDetail/71-CRCW0603100KJNEAC) |       |

For the next-gen design, these are needed. This list is not exhaustive, and still a WIP.

| Qty | Name              | Source                                                                           | Notes                            |
|-----|-------------------|----------------------------------------------------------------------------------|----------------------------------|
| 5   | BSS84 MOSFET      | [750-BSS84-HF](https://www.mouser.dk/ProductDetail/750-BSS84-HF)                 |                                  |
| 5   | USB-C Receptacle  | [640-USB4105-GF-A](https://www.mouser.dk/ProductDetail/640-USB4105-GF-A)         |                                  |
| 5   | 0603 1M resistor  | [71-CRCW02011M00FNED](https://www.mouser.dk/ProductDetail/71-CRCW02011M00FNED)   |                                  |
| 10  | 0603 5k1 resistor | [71-CRCW02015K10FKED](https://www.mouser.dk/ProductDetail/71-CRCW02015K10FKED)   |                                  |
| 5   | Ferrite bead      | [810-MMZ0603Y241CTD25](https://www.mouser.dk/ProductDetail/810-MMZ0603Y241CTD25) | Does the ohm rating matter?      |
| 5   | Push button       | [506-FSMSM](https://www.mouser.dk/ProductDetail/506-FSMSM)                       |                                  |
| 5   | MicroSD card slot | [538-104031-0811](https://www.mouser.dk/ProductDetail/538-104031-0811)           |                                  |
| 5   | RP2040 Stamp      | [Tindie link](https://www.tindie.com/products/arturo182/rp2040-stamp/)           | We're probably phasing this out. |

## Assembling the board
This is TODO until jchillerup has received his boards and has been able to snap some pictures.

