#pragma once
/// @file clock.hpp
/// Monotonic clock utilities for deterministic timing.
/// Uses CLOCK_MONOTONIC — immune to NTP adjustments.

#include <cstdint>
#include <time.h>

namespace pg {

/// Nanosecond timestamp from CLOCK_MONOTONIC.
[[nodiscard]] inline std::int64_t now_ns() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

/// Microsecond timestamp.
[[nodiscard]] inline std::int64_t now_us() noexcept {
    return now_ns() / 1'000;
}

/// Sleep until an absolute nanosecond deadline.
/// Returns actual wakeup time (for jitter measurement).
inline std::int64_t sleep_until_ns(std::int64_t deadline_ns) noexcept {
    struct timespec ts;
    ts.tv_sec = deadline_ns / 1'000'000'000LL;
    ts.tv_nsec = deadline_ns % 1'000'000'000LL;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
    return now_ns();
}

/// Convert milliseconds to nanoseconds.
[[nodiscard]] constexpr std::int64_t ms_to_ns(std::int64_t ms) noexcept {
    return ms * 1'000'000LL;
}

/// Convert microseconds to nanoseconds.
[[nodiscard]] constexpr std::int64_t us_to_ns(std::int64_t us) noexcept {
    return us * 1'000LL;
}

} // namespace pg
