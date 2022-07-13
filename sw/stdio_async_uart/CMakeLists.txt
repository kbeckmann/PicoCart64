add_library(stdio_async_uart INTERFACE)

target_sources(stdio_async_uart INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/stdio_async_uart.c
)

target_include_directories(stdio_async_uart INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(stdio_async_uart INTERFACE pico_stdio)