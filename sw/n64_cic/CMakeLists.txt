add_library(n64_cic INTERFACE)

target_sources(n64_cic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/n64_cic.c
)

target_include_directories(n64_cic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
