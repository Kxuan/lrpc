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

set(TEST_SOURCES check_lrpc.c 01core.c test_common.h 02thread.c)

add_executable(check_lrpc ${TEST_SOURCES})
target_link_libraries(check_lrpc lrpc pthread ${CHECK_LIBRARIES})
