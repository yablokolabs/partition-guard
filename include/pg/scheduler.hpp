#pragma once
/// @file scheduler.hpp
/// Cyclic executive: runs partitions according to a fixed time schedule.
/// Enforces temporal budgets with nanosecond precision.

#include <cstdint>
#include <atomic>
#include "pg/config.hpp"
#include "pg/partition.hpp"
#include "pg/trace.hpp"
#include "pg/health_monitor.hpp"

namespace pg {

/// Scheduler statistics.
struct SchedulerStats {
    std::uint64_t major_frames;      ///< Total major frames executed
    std::uint64_t total_overruns;    ///< Total budget overruns across all partitions
    std::int64_t  worst_jitter_ns;   ///< Worst major frame jitter
    std::int64_t  total_runtime_ns;  ///< Total scheduler runtime
};

/// Cyclic executive scheduler.
class Scheduler {
public:
    /// Initialize scheduler with configuration.
    /// Partitions must be registered before calling run().
    explicit Scheduler(const ScheduleConfig& config) noexcept;

    /// Register a workload function for a partition.
    void register_workload(std::uint8_t partition_id, WorkloadFn fn) noexcept;

    /// Run the scheduler for a fixed number of major frames.
    /// Blocks until complete or stop() is called.
    void run(std::uint64_t num_frames) noexcept;

    /// Run indefinitely until stop() is called.
    void run_forever() noexcept;

    /// Signal the scheduler to stop after the current major frame.
    void stop() noexcept;

    /// Get scheduler statistics.
    [[nodiscard]] const SchedulerStats& stats() const noexcept { return stats_; }

    /// Get partition runtime info.
    [[nodiscard]] const Partition& partition(std::uint8_t id) const noexcept {
        return partitions_[id];
    }

    /// Get health monitor (const).
    [[nodiscard]] const HealthMonitor& health() const noexcept { return health_; }

    /// Get trace log.
    [[nodiscard]] TraceLog& trace() noexcept { return trace_; }

private:
    /// Execute one major frame.
    void execute_major_frame() noexcept;

    /// Execute one window (single partition time slot).
    void execute_window(const Window& window) noexcept;

    /// Pin scheduler thread to configured CPU core.
    void pin_cpu() noexcept;

    ScheduleConfig config_;
    Partition partitions_[MAX_PARTITIONS];
    HealthMonitor health_;
    TraceLog trace_;
    SchedulerStats stats_;
    std::atomic<bool> running_;
};

} // namespace pg
