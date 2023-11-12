cmake_minimum_required(VERSION 3.26)
project(stegano C)

set(CMAKE_C_STANDARD 17)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/..)

add_library(C_extension SHARED
        library.h
        library.c
)
