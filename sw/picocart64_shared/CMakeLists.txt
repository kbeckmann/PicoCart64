add_library(picocart64_shared INTERFACE)

target_sources(picocart64_shared INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pc64_rand.c
)

target_include_directories(picocart64_shared INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
