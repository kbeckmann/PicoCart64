add_executable(picocart64
    cic.c
    n64_pi.c
    picocart64.c
    sram.c
)

if(NOT DEFINED REGION)
    message(FATAL_ERROR "Please set the variable REGION to either NTSC or PAL")
endif()

if(REGION STREQUAL "NTSC")
    set(CONFIG_REGION_NTSC 1)
    set(CONFIG_REGION_PAL 0)
elseif(REGION STREQUAL "PAL")
    set(CONFIG_REGION_NTSC 0)
    set(CONFIG_REGION_PAL 1)
else()
    message(FATAL_ERROR "Please set the variable REGION to either NTSC or PAL")
endif()


target_compile_definitions(picocart64 PRIVATE
    PICO_STDOUT_MUTEX=0
    CONFIG_REGION_NTSC=${CONFIG_REGION_NTSC}
    CONFIG_REGION_PAL=${CONFIG_REGION_PAL}
)


# Generate git_info.h
set(GIT_INFO_H ${CMAKE_CURRENT_BINARY_DIR}/git_info.h)

add_custom_target(update_git_info DEPENDS ${GIT_INFO_H})

add_custom_command(OUTPUT ${GIT_INFO_H}
        COMMENT "Generating ${GIT_INFO_H}"
        COMMAND ${CMAKE_COMMAND} -E echo_append " #define GIT_REV 0x" > ${GIT_INFO_H}
        COMMAND git rev-parse --short=8 HEAD >> ${GIT_INFO_H}
)


target_include_directories(picocart64 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(picocart64 update_git_info)

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

