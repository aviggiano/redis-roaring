cmake_minimum_required(VERSION 3.2)
project(reroaring)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)

include_directories(${CROARING_PATH})

set(SOURCE_FILES ${SRC_PATH}/reroaring.c)
add_library(reroaring SHARED ${SOURCE_FILES})
target_link_libraries(reroaring roaring)
