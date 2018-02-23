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

add_library(lrpc SHARED
        func_table.c func_table.h
        interface.c interface.h
        msg.c msg.h
        endpoint.c endpoint.h
        ../include/lrpc-internal.h ../include/lrpc.h
        )

target_link_libraries(lrpc PUBLIC pthread)
target_compile_options(lrpc PUBLIC -fvisibility=hidden)
install(TARGETS lrpc DESTINATION lib)