#pragma once
/// @file health_monitor.hpp
/// Health monitoring and fault management for partitions.
/// Tracks budget overruns, decides restart/halt policy.

#include <cstdint>
#include <array>
#include "pg/config.hpp"
#include "pg/trace.hpp"

namespace pg {

/// Health status for a single partition.
struct PartitionHealth {
    std::uint32_t overrun_count;      ///< Total budget overruns
    std::uint32_t restart_count;      ///< Times partition was restarted
    std::int64_t  last_overrun_ns;    ///< Timestamp of last overrun
    std::int64_t  worst_overrun_ns;   ///< Worst overrun amount
    bool          faulted;            ///< Currently in fault state
};

/// Health monitor actions.
enum class HealthAction : std::uint8_t {
    None,           ///< No action needed
    LogWarning,     ///< Log but continue
    RestartPartition, ///< Restart the faulted partition
    HaltSystem,     ///< Critical partition failed — halt everything
};

/// Health monitor: tracks partition health, decides on fault response.
class HealthMonitor {
public:
    /// Maximum consecutive overruns before restart.
    static constexpr std::uint32_t OVERRUN_THRESHOLD = 3;
    /// Maximum restarts before declaring permanent fault.
    static constexpr std::uint32_t MAX_RESTARTS = 5;

    HealthMonitor() noexcept {
        for (auto& h : health_) {
            h = PartitionHealth{0, 0, 0, 0, false};
        }
        consecutive_overruns_.fill(0);
    }

    /// Report a budget overrun. Returns recommended action.
    [[nodiscard]] HealthAction report_overrun(
        std::uint8_t partition_id,
        std::int64_t overrun_ns,
        std::int64_t timestamp_ns,
        bool is_critical,
        TraceLog* trace = nullptr
    ) noexcept {
        if (partition_id >= MAX_PARTITIONS) return HealthAction::None;

        auto& h = health_[partition_id];
        h.overrun_count++;
        h.last_overrun_ns = timestamp_ns;
        if (overrun_ns > h.worst_overrun_ns) {
            h.worst_overrun_ns = overrun_ns;
        }
        consecutive_overruns_[partition_id]++;

        if (trace) {
            trace->record(TraceEvent::BudgetOverrun, partition_id,
                         timestamp_ns, static_cast<std::int32_t>(overrun_ns / 1000));
        }

        // Decision logic
        if (consecutive_overruns_[partition_id] >= OVERRUN_THRESHOLD) {
            if (h.restart_count >= MAX_RESTARTS) {
                h.faulted = true;
                if (is_critical) {
                    if (trace) {
                        trace->record(TraceEvent::HealthFault, partition_id, timestamp_ns);
                    }
                    return HealthAction::HaltSystem;
                }
                return HealthAction::LogWarning; // Non-critical: log and skip
            }
            h.restart_count++;
            consecutive_overruns_[partition_id] = 0;
            if (trace) {
                trace->record(TraceEvent::HealthRestart, partition_id, timestamp_ns);
            }
            return HealthAction::RestartPartition;
        }

        return HealthAction::LogWarning;
    }

    /// Report successful execution (clears consecutive overrun counter).
    void report_success(std::uint8_t partition_id) noexcept {
        if (partition_id < MAX_PARTITIONS) {
            consecutive_overruns_[partition_id] = 0;
        }
    }

    /// Get health status for a partition.
    [[nodiscard]] const PartitionHealth& status(std::uint8_t partition_id) const noexcept {
        return health_[partition_id];
    }

private:
    std::array<PartitionHealth, MAX_PARTITIONS> health_;
    std::array<std::uint32_t, MAX_PARTITIONS> consecutive_overruns_;
};

} // namespace pg
