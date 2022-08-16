add_executable(picocart64_v1
    n64_pi_task.c
    picocart64_v1.c
    sram.c
    utils.c
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
    set(N64_ROM_SIZE "1856k")
elseif(FLASH_SIZE_MB STREQUAL "4")
    set(N64_ROM_SIZE "3904k")
elseif(FLASH_SIZE_MB STREQUAL "8")
    set(N64_ROM_SIZE "8000k")
elseif(FLASH_SIZE_MB STREQUAL "16")
    set(N64_ROM_SIZE "16192k")
endif()

target_link_options(picocart64_v1 PRIVATE
    "-Wl,--defsym=__N64_ROM_LENGTH__=${N64_ROM_SIZE}")

target_compile_definitions(picocart64_v1 PRIVATE
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
        COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} rev-parse --short=8 HEAD >> ${GIT_INFO_H}
)


target_include_directories(picocart64_v1 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../generated
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(picocart64_v1 update_git_info)

# pull in common dependencies
target_link_libraries(picocart64_v1
    n64_cic
    n64_pi
    picocart64_shared
    pico_multicore
    pico_stdlib
    stdio_async_uart
    hardware_flash
    FreeRTOS-Kernel-Static # FreeRTOS kernel, static allocations
)

# disable usb output (interrupt heavy)
pico_enable_stdio_usb(picocart64_v1 0)

# disable uart output
pico_enable_stdio_uart(picocart64_v1 0)

# use custom linker script
pico_set_linker_script(picocart64_v1 ${CMAKE_CURRENT_LIST_DIR}/memmap_custom.ld)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picocart64_v1)

