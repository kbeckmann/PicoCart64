add_executable(picocart64_v2
    picocart64_v2.c
    mcu1.c
    mcu2.c
    ws2812b.c
)

#####################
# Generate git_info.h
set(GIT_INFO_H ${CMAKE_CURRENT_BINARY_DIR}/git_info.h)

add_custom_target(update_git_info2 DEPENDS ${GIT_INFO_H})

add_custom_command(OUTPUT ${GIT_INFO_H}
        COMMENT "Generating ${GIT_INFO_H}"
        COMMAND ${CMAKE_COMMAND} -E echo_append " #define GIT_REV 0x" > ${GIT_INFO_H}
        COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} rev-parse --short=8 HEAD >> ${GIT_INFO_H}
)
#####################


target_include_directories(picocart64_v2 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../generated
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(picocart64_v2
    update_git_info2
)

# pull in common dependencies
target_link_libraries(picocart64_v2
    picocart64_shared
    pico_multicore
    pico_stdlib
    stdio_async_uart
    hardware_flash
    FreeRTOS-Kernel-Static # FreeRTOS kernel, static allocations
)

# stdio config
pico_enable_stdio_usb(picocart64_v2 1)
pico_enable_stdio_uart(picocart64_v2 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64_v2)
