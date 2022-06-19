To build and flash with a j-link:

```bash

# Prepare a rom
xxd -i my_rom.z64 > cic_test/rom.h
# Change declaration to:
# const unsigned char __in_flash("rom_file") rom_file[] = {


mkdir build
cd build
cmake  -DPICO_COPY_TO_RAM=1 ..
make


# Requires https://github.com/raspberrypi/openocd
# AUR package if you're on archlinux: https://aur.archlinux.org/packages/openocd-raspberrypi-git
openocd -f interface/jlink.cfg -c "transport select swd; adapter_khz 1000" -f target/rp2040.cfg -c "program ./cic_test/cic_test.elf verify reset exit"

```
