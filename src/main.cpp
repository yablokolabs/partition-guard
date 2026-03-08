/// @file main.cpp
/// partition-guard demo: runs 3 partitions for a configurable number of frames.
///
/// Usage:
///   partition-guard [num_frames] [--cpu N]
///
/// Example:
///   partition-guard 100          # Run 100 major frames (~3 seconds)
///   partition-guard 1000 --cpu 2 # 1000 frames pinned to CPU 2

#include "pg/scheduler.hpp"
#include "pg/config.hpp"
#include "pg/partition.hpp"
#include "pg/clock.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

static pg::Scheduler* g_scheduler = nullptr;

static void signal_handler(int /*sig*/) {
    if (g_scheduler) g_scheduler->stop();
}

static void print_banner() {
    std::fprintf(stderr,
        "╔══════════════════════════════════════════════╗\n"
        "║         partition-guard v0.1.0               ║\n"
        "║   Userspace partition scheduler with         ║\n"
        "║   temporal and spatial isolation             ║\n"
        "╚══════════════════════════════════════════════╝\n\n"
    );
}

static void print_config(const pg::ScheduleConfig& cfg) {
    std::fprintf(stderr, "Schedule:\n");
    std::fprintf(stderr, "  Major frame: %ldms\n", cfg.major_frame_ns / 1'000'000);
    std::fprintf(stderr, "  Partitions:  %zu\n", cfg.num_partitions);
    std::fprintf(stderr, "  Windows:     %zu\n", cfg.num_windows);
    std::fprintf(stderr, "  CPU pin:     %s\n",
                cfg.cpu_affinity >= 0 ? "yes" : "no (use --cpu N)");
    std::fprintf(stderr, "  Trace:       %s\n\n",
                cfg.trace_enabled ? cfg.trace_path : "disabled");

    std::fprintf(stderr, "  %-12s %-10s %-10s %-8s\n", "Partition", "Budget", "Offset", "Critical");
    std::fprintf(stderr, "  %-12s %-10s %-10s %-8s\n", "---------", "------", "------", "--------");
    for (std::size_t i = 0; i < cfg.num_windows; ++i) {
        auto& w = cfg.windows[i];
        auto& p = cfg.partitions[w.partition_id];
        std::fprintf(stderr, "  %-12s %-10ld %-10ld %-8s\n",
                    p.name,
                    w.duration_ns / 1'000'000,
                    w.offset_ns / 1'000'000,
                    p.critical ? "YES" : "no");
    }
    std::fprintf(stderr, "\n");
}

static void print_results(const pg::Scheduler& sched, const pg::ScheduleConfig& cfg) {
    auto& stats = sched.stats();

    std::fprintf(stderr, "\n══════════════════════════════════════════════\n");
    std::fprintf(stderr, "Results:\n");
    std::fprintf(stderr, "  Major frames:    %lu\n", stats.major_frames);
    std::fprintf(stderr, "  Total runtime:   %.3fs\n",
                static_cast<double>(stats.total_runtime_ns) / 1e9);
    std::fprintf(stderr, "  Total overruns:  %lu\n", stats.total_overruns);
    std::fprintf(stderr, "  Worst jitter:    %ldμs\n", stats.worst_jitter_ns / 1000);
    std::fprintf(stderr, "\n");

    std::fprintf(stderr, "  %-12s %-8s %-10s %-10s %-10s %-8s\n",
                "Partition", "Iters", "Last(μs)", "Worst(μs)", "Avg(μs)", "Overruns");
    std::fprintf(stderr, "  %-12s %-8s %-10s %-10s %-10s %-8s\n",
                "---------", "-----", "--------", "---------", "-------", "--------");

    for (std::size_t i = 0; i < cfg.num_partitions; ++i) {
        auto& p = sched.partition(static_cast<std::uint8_t>(i));
        auto& h = sched.health().status(static_cast<std::uint8_t>(i));
        auto avg_us = p.iteration > 0
            ? static_cast<std::int64_t>(p.total_exec_ns / p.iteration / 1000)
            : 0;

        std::fprintf(stderr, "  %-12s %-8lu %-10ld %-10ld %-10ld %-8u\n",
                    p.name,
                    p.iteration,
                    p.last_exec_ns / 1000,
                    p.worst_exec_ns / 1000,
                    avg_us,
                    h.overrun_count);
    }
    std::fprintf(stderr, "══════════════════════════════════════════════\n");
}

int main(int argc, char* argv[]) {
    print_banner();

    // Parse arguments
    std::uint64_t num_frames = 100; // default: 100 frames (~3s at 30ms/frame)
    int cpu_pin = -1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            cpu_pin = std::atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            num_frames = static_cast<std::uint64_t>(std::atol(argv[i]));
        }
    }

    // Build configuration
    auto cfg = pg::default_config();
    cfg.cpu_affinity = cpu_pin;

    print_config(cfg);

    // Create scheduler
    pg::Scheduler scheduler(cfg);
    g_scheduler = &scheduler;

    // Register workloads
    scheduler.register_workload(0, pg::workload_nav);
    scheduler.register_workload(1, pg::workload_sensor);
    scheduler.register_workload(2, pg::workload_display);

    // Handle Ctrl+C gracefully
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::fprintf(stderr, "Running %lu major frames (%.1fs expected)...\n\n",
                num_frames,
                static_cast<double>(num_frames * cfg.major_frame_ns) / 1e9);

    // Run
    scheduler.run(num_frames);

    // Report
    print_results(scheduler, cfg);

    return 0;
}
