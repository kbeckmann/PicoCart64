add_executable(picocart64
    picocart64.c
    cic.c
)

# pull in common dependencies
target_link_libraries(
    picocart64
    pico_multicore
    pico_stdlib
    hardware_pio
)

# Build pio
pico_generate_pio_header(picocart64 ${CMAKE_CURRENT_LIST_DIR}/n64_pi.pio)

# disable usb output (interrupt heavy)
pico_enable_stdio_usb(picocart64 0)

# enable uart output
pico_enable_stdio_uart(picocart64 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64)

