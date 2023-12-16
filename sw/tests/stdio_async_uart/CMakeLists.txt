cmake_minimum_required(VERSION 3.10)

# Set the project name
project(test_ringbuf)

# Add the executable
add_executable(test_ringbuf test_ringbuf.c)

# Specify the directories where the compiler can find the headers
include_directories(${CMAKE_SOURCE_DIR}/../../../sw/stdio_async_uart/include)

# If there are any libraries in your main project that the test needs to link against,
# you can specify them here
# target_link_libraries(test_ringbuf your_library)