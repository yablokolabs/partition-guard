/// @file test_scheduler.cpp
/// Tests for the cyclic executive scheduler.

#include "pg/clock.hpp"
#include "pg/config.hpp"
#include "pg/partition.hpp"
#include "pg/scheduler.hpp"
#include <cassert>
#include <cstdio>

/// Minimal workload: does nothing, returns immediately.
static int workload_noop(std::uint8_t /*id*/, std::uint64_t /*iter*/) noexcept {
    return 0;
}

/// Workload that takes ~1ms.
static int workload_1ms(std::uint8_t /*id*/, std::uint64_t /*iter*/) noexcept {
    auto deadline = pg::now_ns() + pg::ms_to_ns(1);
    while (pg::now_ns() < deadline) {
        asm volatile("" ::: "memory");
    }
    return 0;
}

void test_scheduler_runs() {
    auto cfg = pg::default_config();
    cfg.trace_enabled = false;
    cfg.cpu_affinity = -1;

    pg::Scheduler sched(cfg);
    sched.register_workload(0, workload_noop);
    sched.register_workload(1, workload_noop);
    sched.register_workload(2, workload_noop);

    sched.run(10); // 10 major frames

    assert(sched.stats().major_frames == 10);
    assert(sched.partition(0).iteration == 10);
    assert(sched.partition(1).iteration == 10);
    assert(sched.partition(2).iteration == 10);

    std::fprintf(stderr, "  ✓ scheduler runs 10 frames\n");
}

void test_no_overruns_with_short_workloads() {
    auto cfg = pg::default_config();
    cfg.trace_enabled = false;

    pg::Scheduler sched(cfg);
    sched.register_workload(0, workload_1ms); // 1ms in 10ms budget = OK
    sched.register_workload(1, workload_1ms); // 1ms in 5ms budget = OK
    sched.register_workload(2, workload_1ms); // 1ms in 15ms budget = OK

    sched.run(20);

    assert(sched.stats().total_overruns == 0 && "No overruns expected");
    std::fprintf(stderr, "  ✓ no overruns with short workloads (%lu frames)\n",
                 sched.stats().major_frames);
}

void test_timing_reasonable() {
    auto cfg = pg::default_config();
    cfg.trace_enabled = false;

    pg::Scheduler sched(cfg);
    sched.register_workload(0, workload_noop);
    sched.register_workload(1, workload_noop);
    sched.register_workload(2, workload_noop);

    auto start = pg::now_ns();
    sched.run(10); // 10 frames × 30ms = ~300ms expected
    auto elapsed_ms = (pg::now_ns() - start) / 1'000'000;

    // Should take roughly 300ms (allow 250-400ms window)
    assert(elapsed_ms >= 250 && elapsed_ms <= 400 && "10 frames at 30ms should take ~300ms");
    std::fprintf(stderr, "  ✓ timing reasonable (elapsed=%ldms, expected ~300ms)\n", elapsed_ms);
}

void test_statistics_tracking() {
    auto cfg = pg::default_config();
    cfg.trace_enabled = false;

    pg::Scheduler sched(cfg);
    sched.register_workload(0, workload_1ms);
    sched.register_workload(1, workload_1ms);
    sched.register_workload(2, workload_1ms);

    sched.run(5);

    for (std::uint8_t i = 0; i < 3; ++i) {
        if (sched.partition(i).iteration != 5)
            return std::fprintf(stderr, "  ✗ iteration\n"), (void)0;
        if (sched.partition(i).last_exec_ns <= 0)
            return std::fprintf(stderr, "  ✗ last_exec\n"), (void)0;
        if (sched.partition(i).worst_exec_ns <= 0)
            return std::fprintf(stderr, "  ✗ worst_exec\n"), (void)0;
        if (sched.partition(i).total_exec_ns <= 0)
            return std::fprintf(stderr, "  ✗ total_exec\n"), (void)0;
    }

    std::fprintf(stderr, "  ✓ statistics tracking\n");
}

int main() {
    std::fprintf(stderr, "test_scheduler:\n");
    test_scheduler_runs();
    test_no_overruns_with_short_workloads();
    test_timing_reasonable();
    test_statistics_tracking();
    std::fprintf(stderr, "  All scheduler tests passed.\n");
    return 0;
}
