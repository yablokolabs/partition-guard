#pragma once
/// @file config.hpp
/// Schedule configuration structures.
/// Parsed from YAML/JSON by Python tooling, consumed as C structs here.
/// For the reference implementation, we use a simple compiled-in config.

#include <cstdint>
#include <array>

namespace pg {

/// Maximum supported partitions.
static constexpr std::size_t MAX_PARTITIONS = 8;

/// Maximum windows (time slots) per major frame.
static constexpr std::size_t MAX_WINDOWS = 32;

/// A single time window in the cyclic schedule.
struct Window {
    std::uint8_t  partition_id;   ///< Which partition runs in this window
    std::int64_t  offset_ns;     ///< Offset from major frame start
    std::int64_t  duration_ns;   ///< Budget for this window
};

/// Partition descriptor.
struct PartitionDesc {
    std::uint8_t  id;
    const char*   name;           ///< Human-readable name (for tracing)
    std::int64_t  period_ns;      ///< Expected execution period
    std::int64_t  budget_ns;      ///< Maximum allowed execution time per window
    int           priority;       ///< Partition priority (for health decisions)
    bool          critical;       ///< If true, system halts on unrecoverable fault
};

/// Complete schedule configuration.
struct ScheduleConfig {
    std::int64_t  major_frame_ns;              ///< Major frame duration
    std::size_t   num_partitions;
    std::size_t   num_windows;
    std::array<PartitionDesc, MAX_PARTITIONS>  partitions;
    std::array<Window, MAX_WINDOWS>            windows;
    int           cpu_affinity;                ///< CPU core to pin scheduler (-1 = no pin)
    bool          trace_enabled;
    const char*   trace_path;
};

/// Build a default 3-partition example configuration.
/// NAV: 10ms budget, SENSOR: 5ms budget, DISPLAY: 15ms budget
/// Major frame: 30ms
inline ScheduleConfig default_config() noexcept {
    ScheduleConfig cfg{};
    cfg.major_frame_ns = 30'000'000; // 30ms
    cfg.num_partitions = 3;
    cfg.num_windows    = 3;
    cfg.cpu_affinity   = -1;
    cfg.trace_enabled  = true;
    cfg.trace_path     = "partition-guard.trace";

    cfg.partitions[0] = {0, "NAV",     10'000'000, 10'000'000, 2, false};
    cfg.partitions[1] = {1, "SENSOR",   5'000'000,  5'000'000, 3, true};
    cfg.partitions[2] = {2, "DISPLAY", 15'000'000, 15'000'000, 1, false};

    cfg.windows[0] = {0,          0, 10'000'000}; // NAV:     0-10ms
    cfg.windows[1] = {1, 10'000'000,  5'000'000}; // SENSOR: 10-15ms
    cfg.windows[2] = {2, 15'000'000, 15'000'000}; // DISPLAY: 15-30ms

    return cfg;
}

} // namespace pg
