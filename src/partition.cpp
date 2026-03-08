/// @file partition.cpp
/// Example partition workloads.
/// These simulate real avionics workloads with calibrated busy-wait loops.

#include "pg/partition.hpp"
#include "pg/clock.hpp"

namespace pg {

/// Busy-wait for approximately `duration_ns` nanoseconds.
/// Uses clock_gettime to be accurate regardless of CPU frequency.
static void busy_wait_ns(std::int64_t duration_ns) noexcept {
    auto start = now_ns();
    auto deadline = start + duration_ns;
    while (now_ns() < deadline) {
        // Simulate real computation: prevent compiler from optimizing away
        asm volatile("" ::: "memory");
    }
}

/// Navigation workload: ~3-8ms (varies with iteration for realism).
int workload_nav(std::uint8_t /*id*/, std::uint64_t iteration) noexcept {
    // Vary between 3ms and 8ms based on iteration
    auto base_us = 3000 + static_cast<std::int64_t>((iteration * 7919) % 5000);
    busy_wait_ns(us_to_ns(base_us));
    return 0;
}

/// Sensor fusion workload: ~1-4ms.
int workload_sensor(std::uint8_t /*id*/, std::uint64_t iteration) noexcept {
    auto base_us = 1000 + static_cast<std::int64_t>((iteration * 6271) % 3000);
    busy_wait_ns(us_to_ns(base_us));
    return 0;
}

/// Display update workload: ~5-12ms.
int workload_display(std::uint8_t /*id*/, std::uint64_t iteration) noexcept {
    auto base_us = 5000 + static_cast<std::int64_t>((iteration * 4999) % 7000);
    busy_wait_ns(us_to_ns(base_us));
    return 0;
}

/// Overrun workload: always exceeds any reasonable budget (50ms).
int workload_overrun(std::uint8_t /*id*/, std::uint64_t /*iteration*/) noexcept {
    busy_wait_ns(ms_to_ns(50));
    return 0;
}

} // namespace pg
