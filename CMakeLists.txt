cmake_minimum_required(VERSION 3.20)
project(backtest-engine VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Core library (Phase 0: placeholder sources are fine)
add_library(backtest_core
  src/types.cpp
  src/instrument.cpp
  src/account.cpp
  src/exchange.cpp
  src/metrics.cpp
  src/data_loader.cpp
  src/engine.cpp
)
target_include_directories(backtest_core PUBLIC include)
target_compile_options(backtest_core PRIVATE -Wall -Wextra -Wpedantic)
# CLI
add_executable(backtest src/main.cpp)
target_link_libraries(backtest PRIVATE backtest_core)

# Reproducible native throughput baseline (build Release for meaningful data).
add_executable(bench_tick_engine benchmarks/bench_tick_engine.cpp)
target_link_libraries(bench_tick_engine PRIVATE backtest_core)
# Tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_subdirectory(tests)
