cmake_minimum_required(VERSION 3.2)
project(redis-roaring)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)
set(CROARING_BUILD_PATH ${DEPS_PATH}/CRoaring/build/src)

link_directories(${CROARING_BUILD_PATH})
include_directories(${CROARING_PATH})
set(CMAKE_BUILD_TYPE Debug)

####### unit tests #######
message(STATUS "Compiling unit tests")
set(CMAKE_C_FLAGS_RELEASE "") # removes -NDEBUG

include_directories(${SRC_PATH})
enable_testing()
set(TEST_FILES
        ${SRC_PATH}/data-structure.c
        ${TEST_PATH}/main.c)

add_executable(test_redis-roaring ${TEST_FILES})
target_link_libraries(test_redis-roaring roaring)
add_test(NAME basic_tests COMMAND test_redis-roaring)

add_custom_command(TARGET test_redis-roaring POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
        "Message after yourtarget has been built.")

####### redis module #######
message(STATUS "Compiling redis module")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES ${SRC_PATH}/redis-roaring.c
        ${SRC_PATH}/type.c
        ${SRC_PATH}/data-structure.c)
add_library(redis-roaring SHARED ${SOURCE_FILES})
target_link_libraries(redis-roaring roaring)
