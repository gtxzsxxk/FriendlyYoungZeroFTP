cmake_minimum_required(VERSION 3.22.1)
project(server C)

include_directories(include/)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

add_executable(server src/main.c
        src/listener.c
        include/listener.h
        src/logger.c
        include/logger.h
        src/protocol.c
        include/protocol.h
        src/ftp.c
        include/ftp.h
        src/filesystem.c
        include/filesystem.h
        src/pasv_channel.c
        include/pasv_channel.h
        src/port_channel.c
        include/port_channel.h)
