cmake_minimum_required(VERSION 3.26)
project(server C)

include_directories(include/)

set(CMAKE_C_STANDARD 11)

add_executable(server src/main.c
        src/listener.c
        include/listener.h
        src/logger.c
        include/logger.h)
