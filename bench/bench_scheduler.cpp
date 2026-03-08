/// @file bench_scheduler.cpp
/// Benchmark: measure scheduler jitter over many frames.

#include "pg/scheduler.hpp"
#include "pg/config.hpp"
#include "pg/partition.hpp"
#include "pg/clock.hpp"
#include <cstdio>

static int workload_1ms(std::uint8_t, std::uint64_t) noexcept {
    auto deadline = pg::now_ns() + pg::ms_to_ns(1);
    while (pg::now_ns() < deadline) { asm volatile("" ::: "memory"); }
    return 0;
}

int main() {
    auto cfg = pg::default_config();
    cfg.trace_enabled = true;
    cfg.trace_path = "bench_scheduler.trace";

    pg::Scheduler sched(cfg);
    sched.register_workload(0, workload_1ms);
    sched.register_workload(1, workload_1ms);
    sched.register_workload(2, workload_1ms);

    std::fprintf(stderr, "Running 1000-frame benchmark...\n");
    sched.run(1000);

    auto& s = sched.stats();
    std::fprintf(stderr, "Frames:       %lu\n", s.major_frames);
    std::fprintf(stderr, "Overruns:     %lu\n", s.total_overruns);
    std::fprintf(stderr, "Worst jitter: %ldμs\n", s.worst_jitter_ns / 1000);
    std::fprintf(stderr, "Runtime:      %.3fs\n", static_cast<double>(s.total_runtime_ns) / 1e9);

    return 0;
}
