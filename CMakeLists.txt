cmake_minimum_required(VERSION 3.5)
project(redis-roaring)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Command line options with default values
option(USE_REDIS_ALLOCATOR "Use redis allocator" ON)
option(DISABLE_TESTS "disables tests in hiredis" ON)
option(BUILD_SHARED_LIBS "use shared libraries" OFF)
option(ENABLE_FUZZING "Enable fuzzing targets (requires Clang)" OFF)
set(HIREDIS_PATH ${DEPS_PATH}/hiredis)

# CRoaring configuration
set(ENABLE_ROARING_TESTS OFF)
set(CROARING_PATH ${DEPS_PATH}/CRoaring)

# Disable LTO when fuzzing (fuzzer instrumentation is incompatible with LTO bitcode)
if(ENABLE_FUZZING)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable LTO for fuzzing" FORCE)
  set(ROARING_BUILD_LTO OFF CACHE BOOL "Disable LTO for fuzzing" FORCE)
endif()

# Include directories
include_directories(
  ${SRC_PATH}
  ${DEPS_PATH}
)

add_subdirectory(${CROARING_PATH} EXCLUDE_FROM_ALL)
add_subdirectory(${HIREDIS_PATH} EXCLUDE_FROM_ALL)

# Enable testing
enable_testing()

# Unit tests executable
add_executable(unit ${SRC_PATH}/data-structure.c ${TEST_PATH}/unit.c)
target_link_libraries(unit roaring::roaring)
add_test(NAME unit_tests COMMAND unit)

# Performance tests executable
add_executable(performance ${TEST_PATH}/performance.c)
set(CROARING_BENCHMARK_DATA_DIR "${CROARING_PATH}/benchmarks/realdata/")
target_compile_definitions(performance PRIVATE CROARING_BENCHMARK_DATA_DIR="${CROARING_BENCHMARK_DATA_DIR}")
target_link_libraries(performance m roaring::roaring hiredis::hiredis)
add_test(NAME performance_tests COMMAND performance)

# Build the shared library
set(REDIS_ROARING_SOURCE_FILES
  ${SRC_PATH}/redis-roaring.c
  ${SRC_PATH}/r_32.c
  ${SRC_PATH}/r_64.c
  ${SRC_PATH}/data-structure.c
  ${SRC_PATH}/parse.c
  ${SRC_PATH}/cmd_info/r_info.c
  ${SRC_PATH}/cmd_info/r64_info.c
)

add_library(redis-roaring SHARED ${REDIS_ROARING_SOURCE_FILES})
target_link_libraries(redis-roaring roaring::roaring hiredis::hiredis)

if(USE_REDIS_ALLOCATOR)
  target_compile_definitions(redis-roaring PRIVATE REDIS_MODULE_TARGET)
endif()

# Fuzzing targets (optional, requires Clang)
if(ENABLE_FUZZING)
  message(STATUS "Fuzzing is enabled")

  # Fuzzing is only supported with Clang
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing requires Clang compiler. Skipping fuzz targets.")
  else()
    # Common fuzzing flags
    set(FUZZ_FLAGS
      -fsanitize=fuzzer,address,undefined
      -fno-sanitize-recover=all
      -g
      -O1
    )

    # List of fuzzer targets
    set(FUZZERS
      fuzz_bitmap_api
      fuzz_bitmap64_api
      fuzz_bitmap_operations
      fuzz_bitmap_serialization
    )

    # Create each fuzzer
    foreach(FUZZER ${FUZZERS})
      add_executable(${FUZZER}
        ${TEST_PATH}/fuzz/${FUZZER}.c
        ${SRC_PATH}/data-structure.c
      )

      target_include_directories(${FUZZER} PRIVATE
        ${TEST_PATH}/fuzz
        ${SRC_PATH}
        ${DEPS_PATH}
      )

      target_compile_options(${FUZZER} PRIVATE ${FUZZ_FLAGS})
      target_link_options(${FUZZER} PRIVATE ${FUZZ_FLAGS})

      target_link_libraries(${FUZZER} PRIVATE roaring::roaring)

      # Set output directory
      set_target_properties(${FUZZER} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/fuzz
      )
    endforeach()

    # Copy corpus directories to build directory
    file(COPY ${TEST_PATH}/fuzz/corpus
         DESTINATION ${CMAKE_BINARY_DIR}/tests/fuzz)

    message(STATUS "Fuzzing targets configured. Build and run with:")
    message(STATUS "  make")
    message(STATUS "  ./tests/fuzz/fuzz_bitmap_api -max_total_time=60")
  endif()
endif()
