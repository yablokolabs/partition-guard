/// @file scheduler.cpp
/// Cyclic executive implementation.
/// Runs partitions in fixed time windows with strict budget enforcement.

#include "pg/scheduler.hpp"
#include "pg/clock.hpp"

#include <cstdio>
#include <cstring>
#include <sched.h>

namespace pg {

Scheduler::Scheduler(const ScheduleConfig& config) noexcept
    : config_(config), stats_{}, running_(false) {
    std::memset(partitions_, 0, sizeof(partitions_));
    for (std::size_t i = 0; i < config_.num_partitions; ++i) {
        auto& p = partitions_[i];
        auto& desc = config_.partitions[i];
        p.id       = desc.id;
        p.name     = desc.name;
        p.workload = nullptr;
        p.state    = PartitionState::Idle;
        p.iteration     = 0;
        p.last_exec_ns  = 0;
        p.worst_exec_ns = 0;
        p.total_exec_ns = 0;
    }
}

void Scheduler::register_workload(std::uint8_t partition_id, WorkloadFn fn) noexcept {
    if (partition_id < MAX_PARTITIONS) {
        partitions_[partition_id].workload = fn;
        partitions_[partition_id].state = PartitionState::Idle;
    }
}

void Scheduler::pin_cpu() noexcept {
    if (config_.cpu_affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(config_.cpu_affinity, &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);

        // Set SCHED_FIFO for real-time priority (requires root or CAP_SYS_NICE)
        struct sched_param param;
        param.sched_priority = 80;
        if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
            // Non-fatal: just means we don't have RT privileges
            std::fprintf(stderr, "[pg] Warning: could not set SCHED_FIFO (run as root for best results)\n");
        }
    }
}

void Scheduler::execute_window(const Window& window) noexcept {
    auto& part = partitions_[window.partition_id];
    auto& desc = config_.partitions[window.partition_id];

    if (part.state == PartitionState::Faulted || part.state == PartitionState::Stopped) {
        return; // Skip faulted/stopped partitions
    }

    if (!part.workload) {
        return; // No workload registered
    }

    // Record start
    auto start_ns = now_ns();
    if (config_.trace_enabled) {
        trace_.record(TraceEvent::PartitionStart, part.id, start_ns);
    }

    // Execute the workload
    part.state = PartitionState::Running;
    int result = part.workload(part.id, part.iteration);
    (void)result; // TODO: handle non-zero return

    auto end_ns = now_ns();
    auto exec_ns = end_ns - start_ns;

    // Record end
    if (config_.trace_enabled) {
        trace_.record(TraceEvent::PartitionEnd, part.id, end_ns,
                     static_cast<std::int32_t>(exec_ns / 1000)); // value = exec time in μs
    }

    // Update statistics
    part.last_exec_ns = exec_ns;
    part.total_exec_ns += exec_ns;
    if (exec_ns > part.worst_exec_ns) {
        part.worst_exec_ns = exec_ns;
    }
    part.iteration++;

    // Budget enforcement
    if (exec_ns > window.duration_ns) {
        auto overrun_ns = exec_ns - window.duration_ns;
        part.state = PartitionState::Overrun;
        stats_.total_overruns++;

        auto action = health_.report_overrun(
            part.id, overrun_ns, end_ns, desc.critical, &trace_);

        switch (action) {
            case HealthAction::RestartPartition:
                std::fprintf(stderr, "[pg] HEALTH: Restarting partition %s (id=%u)\n",
                            part.name, part.id);
                part.state = PartitionState::Idle;
                part.iteration = 0;
                break;
            case HealthAction::HaltSystem:
                std::fprintf(stderr, "[pg] HEALTH: HALT — critical partition %s failed\n",
                            part.name);
                running_.store(false, std::memory_order_release);
                return;
            case HealthAction::LogWarning:
                std::fprintf(stderr, "[pg] WARNING: Partition %s overran by %ldμs\n",
                            part.name, overrun_ns / 1000);
                break;
            default:
                break;
        }
    } else {
        part.state = PartitionState::Completed;
        health_.report_success(part.id);
    }
}

void Scheduler::execute_major_frame() noexcept {
    auto frame_start = now_ns();

    if (config_.trace_enabled) {
        trace_.record(TraceEvent::MajorFrameStart, 0xFF, frame_start,
                     static_cast<std::int32_t>(stats_.major_frames));
    }

    // Execute each window in sequence
    for (std::size_t w = 0; w < config_.num_windows && running_.load(std::memory_order_relaxed); ++w) {
        auto& window = config_.windows[w];

        // Wait until window start time
        auto window_start = frame_start + window.offset_ns;
        if (now_ns() < window_start) {
            sleep_until_ns(window_start);
        }

        execute_window(window);
    }

    // Wait for major frame boundary
    auto frame_end_target = frame_start + config_.major_frame_ns;
    auto actual_end = now_ns();

    if (actual_end < frame_end_target) {
        sleep_until_ns(frame_end_target);
        actual_end = now_ns();
    }

    // Jitter = how far off we are from the target frame boundary
    auto jitter = actual_end - frame_end_target;
    if (jitter > stats_.worst_jitter_ns) {
        stats_.worst_jitter_ns = jitter;
    }

    if (config_.trace_enabled) {
        trace_.record(TraceEvent::MajorFrameEnd, 0xFF, actual_end,
                     static_cast<std::int32_t>(jitter / 1000));
    }

    stats_.major_frames++;
}

void Scheduler::run(std::uint64_t num_frames) noexcept {
    pin_cpu();
    running_.store(true, std::memory_order_release);
    stats_.total_runtime_ns = now_ns();

    for (std::uint64_t f = 0; f < num_frames && running_.load(std::memory_order_relaxed); ++f) {
        execute_major_frame();
    }

    stats_.total_runtime_ns = now_ns() - stats_.total_runtime_ns;
    running_.store(false, std::memory_order_release);

    // Flush trace
    if (config_.trace_enabled && config_.trace_path) {
        auto flushed = trace_.flush(config_.trace_path);
        std::fprintf(stderr, "[pg] Flushed %zu trace records to %s\n", flushed, config_.trace_path);
    }
}

void Scheduler::run_forever() noexcept {
    pin_cpu();
    running_.store(true, std::memory_order_release);
    stats_.total_runtime_ns = now_ns();

    while (running_.load(std::memory_order_relaxed)) {
        execute_major_frame();
    }

    stats_.total_runtime_ns = now_ns() - stats_.total_runtime_ns;

    if (config_.trace_enabled && config_.trace_path) {
        trace_.flush(config_.trace_path);
    }
}

void Scheduler::stop() noexcept {
    running_.store(false, std::memory_order_release);
}

} // namespace pg
