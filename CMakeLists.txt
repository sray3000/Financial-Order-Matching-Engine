cmake_minimum_required(VERSION 3.15)
project(MatchingEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core matching engine library
add_library(matching_core src/Engine.cc src/OrderBook.cc)

target_include_directories(matching_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(matching_core PRIVATE
    -O3
    -march=native
    -Wall
    -Wextra
)

# Main interactive executable
add_executable(matching_engine
    src/main.cc
)

target_link_libraries(matching_engine PRIVATE
    matching_core
)

# Benchmark
add_executable(matching_benchmark
    benchmarks/benchmark.cc
)

target_link_libraries(matching_benchmark PRIVATE
    matching_core
)

# Tests
add_executable(run_tests
    tests/test_engine.cc
)

target_link_libraries(run_tests PRIVATE
    matching_core
)