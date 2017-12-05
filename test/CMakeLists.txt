cmake_minimum_required(VERSION 3.8)
project(test)

set(CMAKE_C_STANDARD 99)

set( cmake_cxx_flags `pkg-config fuse --cflags --libs`)
add_definitions(-D_FILE_OFFSET_BITS=64)
set(SOURCE_FILES sfs_test2.c sfs_api.c disk_emu.c bitmap.c)
add_executable(ClionDebug ${SOURCE_FILES})