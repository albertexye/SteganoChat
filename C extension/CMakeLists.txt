cmake_minimum_required(VERSION 3.26)
project(stegano C)
set(CMAKE_C_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif ()

add_library(stegano SHARED
        library.h
        library.c
)

set_target_properties(stegano PROPERTIES
        DEBUG_POSTFIX "_debug"  # Add a "_d" postfix for debug builds
)
