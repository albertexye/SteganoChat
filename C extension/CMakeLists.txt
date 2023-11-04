cmake_minimum_required(VERSION 3.26)
project(C_extension C)

set(CMAKE_C_STANDARD 17)

add_library(C_extension SHARED
        library.h
        library.cpp
)
add_executable(test
        header.h
)
