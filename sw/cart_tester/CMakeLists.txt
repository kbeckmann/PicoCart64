cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Build as release but with debug symbols
set(CMAKE_BUILD_TYPE Debug)

project(cart_tester C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(cart_tester
    cart_tester.c
)
# pull in common dependencies
target_link_libraries(cart_tester
    pico_stdlib
)

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

pico_add_extra_outputs(${PROJECT_NAME})

# stdio config
pico_enable_stdio_usb(cart_tester 1)
pico_enable_stdio_uart(cart_tester 0)