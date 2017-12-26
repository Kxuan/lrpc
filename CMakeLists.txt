# Copyright (C) 2017 kXuan <kxuanobj@gmail.com>
#
# This file is part of the lrpc.
# The lrpc is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
# The lrpc is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# You should have received a copy of the GNU Lesser General Public
# License along with the lrpc; if not, see
# <http://www.gnu.org/licenses/>.

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
