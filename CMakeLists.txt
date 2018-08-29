cmake_minimum_required(VERSION 3.0)
project(redis-roaring)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)
set(HIREDIS_PATH ${DEPS_PATH}/hiredis)
set(CROARING_BUILD_PATH ${DEPS_PATH}/CRoaring/build)
#set(CMAKE_C_FLAGS_RELEASE "") # removes -NDEBUG
set(CMAKE_BUILD_TYPE Debug)

link_directories(${CROARING_BUILD_PATH})
link_directories(${HIREDIS_PATH})
include_directories(${CROARING_PATH})
include_directories(${HIREDIS_PATH})
include_directories(${SRC_PATH})
enable_testing()

####### unit tests #######
message(STATUS "Compiling unit tests")

set(UNIT_FILES
        ${SRC_PATH}/data-structure.c
        ${TEST_PATH}/unit.c)

add_executable(unit ${UNIT_FILES})
target_link_libraries(unit roaring)
add_test(NAME unit_tests COMMAND unit)

####### performance tests #######
message(STATUS "Compiling performance tests")

set(PERFORMANCE_FILES ${TEST_PATH}/performance.c)

add_executable(performance ${PERFORMANCE_FILES})
target_link_libraries(performance libhiredis.a)
target_link_libraries(performance m)
add_test(NAME performance_tests COMMAND performance)

####### redis module #######
message(STATUS "Compiling redis module")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES ${SRC_PATH}/redis-roaring.c
        ${SRC_PATH}/type.c
        ${SRC_PATH}/data-structure.c
        ${SRC_PATH}/sds.c)
add_library(redis-roaring SHARED ${SOURCE_FILES})
target_link_libraries(redis-roaring roaring)
