#pragma once
/// @file partition.hpp
/// Partition abstraction: a callable workload with timing constraints.
/// In a real IMA system, this would be a separate address space.
/// Here we use function pointers + thread isolation as a demonstration.

#include <cstdint>
#include <functional>

namespace pg {

/// Partition workload function type.
/// Takes partition_id and iteration count, returns 0 on success.
using WorkloadFn = int (*)(std::uint8_t partition_id, std::uint64_t iteration);

/// Partition runtime state.
enum class PartitionState : std::uint8_t {
    Idle,      ///< Not yet started
    Running,   ///< Currently executing
    Completed, ///< Finished current window
    Overrun,   ///< Exceeded budget
    Faulted,   ///< Health monitor declared fault
    Stopped,   ///< Permanently stopped
};

/// Runtime partition instance.
struct Partition {
    std::uint8_t id;
    const char *name;
    WorkloadFn workload;
    PartitionState state;
    std::uint64_t iteration;    ///< Execution count
    std::int64_t last_exec_ns;  ///< Last measured execution time
    std::int64_t worst_exec_ns; ///< Worst observed execution time
    std::int64_t total_exec_ns; ///< Cumulative execution time
};

// ── Example workloads ───────────────────────────────────────

/// Simulated navigation workload (~3-8ms of busy work).
int workload_nav(std::uint8_t id, std::uint64_t iteration) noexcept;

/// Simulated sensor fusion workload (~1-4ms of busy work).
int workload_sensor(std::uint8_t id, std::uint64_t iteration) noexcept;

/// Simulated display update workload (~5-12ms of busy work).
int workload_display(std::uint8_t id, std::uint64_t iteration) noexcept;

/// Workload that intentionally overruns (for testing health monitor).
int workload_overrun(std::uint8_t id, std::uint64_t iteration) noexcept;

} // namespace pg
