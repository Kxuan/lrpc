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
include_directories(../../include)
add_executable(simple-sync-provider sync-provider.c)
add_executable(simple-sync-invoker sync-invoker.c)
add_executable(simple-async-provider async-provider.c)
add_executable(simple-async-invoker async-invoker.c)
target_link_libraries(simple-sync-provider lrpc)
target_link_libraries(simple-sync-invoker lrpc pthread)
target_link_libraries(simple-async-provider lrpc)
target_link_libraries(simple-async-invoker lrpc)