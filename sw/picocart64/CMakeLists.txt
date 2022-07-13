add_executable(picocart64
    cic.c
    n64_pi.c
    picocart64.c
    sram.c
)

target_compile_definitions(picocart64 PRIVATE
    PICO_STDOUT_MUTEX=0
)

target_include_directories(picocart64 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(
    picocart64
    pico_multicore
    pico_stdlib
    stdio_async_uart
    hardware_flash
    hardware_pio
    FreeRTOS-Kernel-Static # FreeRTOS kernel, static allocations
)

# Build pio
pico_generate_pio_header(picocart64 ${CMAKE_CURRENT_LIST_DIR}/n64_pi.pio)

# disable usb output (interrupt heavy)
pico_enable_stdio_usb(picocart64 0)

# disable uart output
pico_enable_stdio_uart(picocart64 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64)

