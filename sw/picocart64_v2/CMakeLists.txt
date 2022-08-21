add_executable(picocart64_v2
    led_task.c
    mcu1.c
    mcu2.c
    picocart64_v2.c
    ws2812.c
)

pico_generate_pio_header(picocart64_v2
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

#####################
# Generate git_info.h
set(GIT_INFO_H ${CMAKE_CURRENT_BINARY_DIR}/git_info.h)

add_custom_target(update_git_info_v2 DEPENDS ${GIT_INFO_H})

add_custom_command(OUTPUT ${GIT_INFO_H}
        COMMENT "Generating ${GIT_INFO_H}"
        COMMAND ${CMAKE_COMMAND} -E echo_append " #define GIT_REV 0x" > ${GIT_INFO_H}
        COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} rev-parse --short=8 HEAD >> ${GIT_INFO_H}
)
#####################


target_compile_definitions(picocart64_v2 PRIVATE
    PICO_STDOUT_MUTEX=0
    CONFIG_REGION_NTSC=${CONFIG_REGION_NTSC}
    CONFIG_REGION_PAL=${CONFIG_REGION_PAL}
)

target_include_directories(picocart64_v2 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../generated
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(picocart64_v2
    update_git_info_v2
)

# pull in common dependencies
target_link_libraries(picocart64_v2
    picocart64_shared
    pico_multicore
    pico_stdlib
    stdio_async_uart
    hardware_flash
    hardware_pio
    FreeRTOS-Kernel-Static # FreeRTOS kernel, static allocations
)

# stdio config
pico_enable_stdio_usb(picocart64_v2 1)
pico_enable_stdio_uart(picocart64_v2 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64_v2)
