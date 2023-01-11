add_library(n64_pi INTERFACE)

target_sources(n64_pi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/n64_pi.c
)

pico_generate_pio_header(n64_pi
    ${CMAKE_CURRENT_LIST_DIR}/n64_pi.pio
)

target_include_directories(n64_pi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(n64_pi INTERFACE
    hardware_pio
)