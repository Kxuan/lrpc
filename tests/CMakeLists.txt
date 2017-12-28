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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
link_directories(${CHECK_LIBRARY_DIRS})

file(GLOB TEST_SOURCES *.c)
foreach (SOURCE_FILE ${TEST_SOURCES})
    get_filename_component(NAME ${SOURCE_FILE} NAME_WE)
    add_executable(${NAME} ${SOURCE_FILE})
    target_link_libraries(${NAME} lrpc pthread ${CHECK_LIBRARIES})
    target_compile_definitions(${NAME} PRIVATE TEST_NAME="${NAME}")
    add_test(NAME ${NAME} COMMAND ${NAME})
endforeach (SOURCE_FILE)

