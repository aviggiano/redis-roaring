cmake_minimum_required(VERSION 3.2)
project(reroaring)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)
set(CROARING_BUILD_PATH ${DEPS_PATH}/CRoaring/build/src)

link_directories(${CROARING_BUILD_PATH})
include_directories(${CROARING_PATH})

if($ENV{TEST})
    set(CMAKE_C_FLAGS_RELEASE "") # removes -NDEBUG

    include_directories(${SRC_PATH})
    enable_testing()
    set(TEST_FILES
            ${SRC_PATH}/data_structure.c
            ${TEST_PATH}/main.c)

    set(CMAKE_BUILD_TYPE Debug)

    add_executable(test_reroaring ${TEST_FILES})
    target_link_libraries(test_reroaring roaring)
    add_test(NAME basic_tests COMMAND test_reroaring)
else ()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    set(SOURCE_FILES ${SRC_PATH}/reroaring.c
            ${SRC_PATH}/type.c
            ${SRC_PATH}/data_structure.c)
    add_library(reroaring SHARED ${SOURCE_FILES})
    target_link_libraries(reroaring roaring)
endif ()
