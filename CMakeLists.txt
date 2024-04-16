cmake_minimum_required(VERSION 3.5)
project(redis-roaring)
set (CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(ENABLE_ROARING_TESTS OFF)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)
add_subdirectory(${CROARING_PATH} EXCLUDE_FROM_ALL)
option(DISABLE_TESTS "disables tests in hiredis" ON)
option(BUILD_SHARED_LIBS "use shared libraries" OFF)
set(HIREDIS_PATH ${DEPS_PATH}/hiredis)
add_subdirectory(${HIREDIS_PATH} EXCLUDE_FROM_ALL)
include_directories(${SRC_PATH})
enable_testing()


add_executable(unit ${SRC_PATH}/data-structure.c ${TEST_PATH}/unit.c)
target_link_libraries(unit roaring::roaring)
add_test(NAME unit_tests COMMAND unit)

add_executable(performance ${TEST_PATH}/performance.c)
set (CROARING_BENCHMARK_DATA_DIR "${CROARING_PATH}/benchmarks/realdata/")
target_compile_definitions(performance PRIVATE CROARING_BENCHMARK_DATA_DIR="${CROARING_BENCHMARK_DATA_DIR}")
target_link_libraries(performance  m roaring::roaring hiredis::hiredis)
add_test(NAME performance_tests COMMAND performance)


set(REDIS_ROARING_SOURCE_FILES ${SRC_PATH}/redis-roaring.c
        ${SRC_PATH}/data-structure.c)
add_library(redis-roaring SHARED ${REDIS_ROARING_SOURCE_FILES})

target_link_libraries(redis-roaring roaring::roaring hiredis::hiredis)
