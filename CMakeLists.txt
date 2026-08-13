cmake_minimum_required(VERSION 3.15)
project(MatchingEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apply high-performance optimization flags globally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra")

# Include header path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define build targets
add_executable(matching_server src/server.cc)
add_executable(matching_benchmark src/benchmark.cc)
add_executable(matching_client src/client.cc)
add_executable(matching_engine src/main.cc)
add_executable(run_tests tests/test_engine.cc)