cmake_minimum_required(VERSION 3.5)
project(reroaring)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES reroaring.c)
add_library(reroaring SHARED ${SOURCE_FILES})