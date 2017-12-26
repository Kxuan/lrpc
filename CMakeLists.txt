cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(lrpc C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_C_STANDARD 99)

## Configure lrpc

set(LRPC_MSG_NAME_PREFIX "LRPC\\x01" CACHE STRING "The prefix of msg_name.")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DHAVE_CONFIG_H)
set(CONFIG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)

enable_testing()
add_test(NAME check_lrpc COMMAND check_lrpc)
