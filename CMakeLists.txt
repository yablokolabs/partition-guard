cmake_minimum_required(VERSION 3.25)
project(partition-guard
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Userspace partition scheduler with temporal and spatial isolation"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warnings
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wno-unused-parameter
    -fno-exceptions        # Real-time: no exceptions in hot path
    -fno-rtti              # Real-time: no RTTI overhead
)

# Release builds: optimize for latency
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -march=native -DNDEBUG)
endif()

# ── Library ──────────────────────────────────────────────────

add_library(pg_core STATIC
    src/clock.cpp
    src/config.cpp
    src/trace.cpp
    src/partition.cpp
    src/port.cpp
    src/health_monitor.cpp
    src/scheduler.cpp
)

target_include_directories(pg_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pg_core PUBLIC pthread rt)

# ── Main executable ──────────────────────────────────────────

add_executable(partition-guard src/main.cpp)
target_link_libraries(partition-guard PRIVATE pg_core)

# ── Tests ────────────────────────────────────────────────────

option(PG_BUILD_TESTS "Build tests" ON)

if(PG_BUILD_TESTS)
    enable_testing()

    # Minimal test framework (no external deps)
    add_executable(test_clock tests/test_clock.cpp)
    target_link_libraries(test_clock PRIVATE pg_core)
    add_test(NAME test_clock COMMAND test_clock)

    add_executable(test_port tests/test_port.cpp)
    target_link_libraries(test_port PRIVATE pg_core)
    add_test(NAME test_port COMMAND test_port)

    add_executable(test_scheduler tests/test_scheduler.cpp)
    target_link_libraries(test_scheduler PRIVATE pg_core)
    add_test(NAME test_scheduler COMMAND test_scheduler)

    add_executable(test_health tests/test_health.cpp)
    target_link_libraries(test_health PRIVATE pg_core)
    add_test(NAME test_health COMMAND test_health)
endif()

# ── Benchmarks ───────────────────────────────────────────────

option(PG_BUILD_BENCH "Build benchmarks" OFF)

if(PG_BUILD_BENCH)
    add_executable(bench_scheduler bench/bench_scheduler.cpp)
    target_link_libraries(bench_scheduler PRIVATE pg_core)

    add_executable(bench_ipc bench/bench_ipc.cpp)
    target_link_libraries(bench_ipc PRIVATE pg_core)
endif()
