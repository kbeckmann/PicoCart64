add_executable(picocart64_v2
    mcu1.c
    mcu2.c
    n64_pi_task.c
    picocart64_v2.c
    psram.c
    qspi_helper.c
    sha256.c
    sram.c

    esp32/esp32_task.c

    led/led_task.c

    sdcard/internal_sd_card.c
    sdcard/hw_config.c
    sdcard/simple.c

    ws2812/ws2812.c

    pio_uart/pio_uart.c
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

if(NOT DEFINED FLASH_SIZE_MB)
    set(FLASH_SIZE_MB 2)
endif()

# Subtract the 64kB used for firmware + 128kB used for SRAM backup
if(FLASH_SIZE_MB STREQUAL "2")
    set(N64_ROM_SIZE "1792k")
elseif(FLASH_SIZE_MB STREQUAL "4")
    set(N64_ROM_SIZE "3840k")
elseif(FLASH_SIZE_MB STREQUAL "8")
    set(N64_ROM_SIZE "7936k")
elseif(FLASH_SIZE_MB STREQUAL "16")
    set(N64_ROM_SIZE "16128k")
endif()

target_link_options(picocart64_v2 PRIVATE
    "-Wl,--defsym=__N64_ROM_LENGTH__=${N64_ROM_SIZE}")

pico_generate_pio_header(picocart64_v2
    ${CMAKE_CURRENT_LIST_DIR}/ws2812/ws2812.pio
)

target_compile_definitions(picocart64_v2 PRIVATE
    PICO_STDOUT_MUTEX=0
    NO_PICO_LED=1
    CONFIG_CIC_YIELD=0
    CONFIG_REGION_NTSC=${CONFIG_REGION_NTSC}
    CONFIG_REGION_PAL=${CONFIG_REGION_PAL}
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



target_include_directories(picocart64_v2 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/esp32
    ${CMAKE_CURRENT_LIST_DIR}/led
    ${CMAKE_CURRENT_LIST_DIR}/sdcard
    ${CMAKE_CURRENT_LIST_DIR}/ws2812

    ${CMAKE_CURRENT_LIST_DIR}/../generated
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(picocart64_v2
    update_git_info_v2
)

# pull in common dependencies
target_link_libraries(picocart64_v2
    n64_cic
    n64_pi
    picocart64_shared
    pico_multicore
    pico_stdlib
    stdio_async_uart
    hardware_flash
    hardware_pio

    # 3rd party libs
    FreeRTOS-Kernel-Static # FreeRTOS kernel, static allocations
    FatFs_SPI # SDCard and FatFS
)

# stdio config
pico_enable_stdio_usb(picocart64_v2 0)
pico_enable_stdio_uart(picocart64_v2 1)

pico_generate_pio_header(picocart64_v2 
    ${CMAKE_CURRENT_LIST_DIR}/pio_uart/pio_uart.pio
)

# use custom linker script
pico_set_linker_script(picocart64_v2 ${CMAKE_CURRENT_LIST_DIR}/memmap_custom.ld)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64_v2)
