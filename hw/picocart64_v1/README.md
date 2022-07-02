# PicoCart64 v1.0 (full)

## Errata

### v1.0
- Silk screen labels for JP1 - JP4 are in the incorrect order. It goes from bottom to top but should go the other way, i.e. correct mapping is:
  - JP1=S_DAT
  - JP2=SI_CLK
  - JP3=EEP_SDAT
  - JP4=NMI
- The signal EEP_SDAT does not exist, i.e. JP3 is unconnected.
- Pin 44 (INT/interrupt pin) is not broken out. This can be fixed with a bodge wire from pin 44 to JP3 (since EEP_SDAT is not a connected net). Pin 44 is on the top layer which is convenient.


## Changelog

### v1.1

- Connected pin 44 to JP3 (was unconnected earlier, see errata).
- Fixed silkscreen:
  - JP1=S_DAT
  - JP2=SI_CLK
  - JP3=INT
  - JP4=NMI
- Added more ground stitching vias to improve the ground plane.
- Improved routing, removed some vias
