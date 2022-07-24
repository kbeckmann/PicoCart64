add_executable(dualpico
    dualpico.c
)

target_include_directories(dualpico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# pull in common dependencies
target_link_libraries(
    dualpico
    pico_stdlib
)

# usb
pico_enable_stdio_usb(dualpico 1)

# uart
pico_enable_stdio_uart(dualpico 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dualpico)

