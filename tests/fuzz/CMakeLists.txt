# Fuzzing targets for redis-roaring
#
# Build with:
#   mkdir build-fuzz && cd build-fuzz
#   CC=clang CXX=clang++ cmake -DENABLE_FUZZING=ON ..
#   make
#
# Run a fuzzer:
#   ./tests/fuzz/fuzz_bitmap_api -max_total_time=60

cmake_minimum_required(VERSION 3.5)

# Fuzzing is only supported with Clang
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing requires Clang compiler. Skipping fuzz targets.")
    return()
endif()

# Set C++ standard for fuzz targets
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common fuzzing flags
set(FUZZ_FLAGS
    -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=all
    -g
    -O1
)

# Common include directories
set(FUZZ_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SRC_PATH}
    ${DEPS_PATH}
)

# Common source files (data-structure.c is needed by all fuzzers)
set(COMMON_SOURCES
    ${SRC_PATH}/data-structure.c
)

# Helper function to create a fuzz target
function(add_fuzz_target target_name source_file)
    add_executable(${target_name} ${source_file} ${COMMON_SOURCES})

    target_include_directories(${target_name} PRIVATE ${FUZZ_INCLUDE_DIRS})

    target_compile_options(${target_name} PRIVATE ${FUZZ_FLAGS})
    target_link_options(${target_name} PRIVATE ${FUZZ_FLAGS})

    target_link_libraries(${target_name} PRIVATE roaring::roaring)

    # Set output directory
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/fuzz
    )
endfunction()

# Create fuzz targets
add_fuzz_target(fuzz_bitmap_api fuzz_bitmap_api.cpp)
add_fuzz_target(fuzz_bitmap64_api fuzz_bitmap64_api.cpp)
add_fuzz_target(fuzz_bitmap_operations fuzz_bitmap_operations.cpp)
add_fuzz_target(fuzz_bitmap_serialization fuzz_bitmap_serialization.cpp)

# Copy corpus directories to build directory for easy access
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/corpus
     DESTINATION ${CMAKE_BINARY_DIR}/tests/fuzz)

# Print instructions
message(STATUS "Fuzzing targets configured. Build and run with:")
message(STATUS "  make")
message(STATUS "  ./tests/fuzz/fuzz_bitmap_api -max_total_time=60")
