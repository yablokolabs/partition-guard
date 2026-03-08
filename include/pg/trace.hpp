#pragma once
/// @file trace.hpp
/// Lock-free trace logger for partition timing events.
/// Writes binary records to a ring buffer, flushed to file on demand.

#include <cstdint>
#include <cstdio>
#include <array>
#include <atomic>

namespace pg {

/// Trace event types.
enum class TraceEvent : std::uint8_t {
    PartitionStart  = 0x01,
    PartitionEnd    = 0x02,
    BudgetOverrun   = 0x03,
    HealthFault     = 0x04,
    HealthRestart   = 0x05,
    MajorFrameStart = 0x10,
    MajorFrameEnd   = 0x11,
    PortWrite       = 0x20,
    PortRead        = 0x21,
};

/// Single trace record (32 bytes, cache-line friendly).
struct TraceRecord {
    std::int64_t timestamp_ns;   // 8 bytes
    TraceEvent   event;          // 1 byte
    std::uint8_t partition_id;   // 1 byte
    std::uint8_t _pad[2];        // 2 bytes padding
    std::int32_t value;          // 4 bytes (budget remaining, latency, etc.)
    std::int64_t extra;          // 8 bytes (additional context)
    std::uint64_t _reserved;     // 8 bytes
};

static_assert(sizeof(TraceRecord) == 32);

/// Fixed-capacity ring buffer for trace records.
/// Single-producer (scheduler thread), single-consumer (flush thread).
class TraceLog {
public:
    static constexpr std::size_t CAPACITY = 1 << 16; // 65536 records = 2MB

    TraceLog() noexcept : head_(0), tail_(0) {}

    /// Record a trace event (lock-free, wait-free).
    void record(TraceEvent event, std::uint8_t partition_id,
                std::int64_t timestamp_ns, std::int32_t value = 0,
                std::int64_t extra = 0) noexcept {
        auto idx = head_.fetch_add(1, std::memory_order_relaxed) % CAPACITY;
        auto& r = buffer_[idx];
        r.timestamp_ns  = timestamp_ns;
        r.event         = event;
        r.partition_id  = partition_id;
        r.value         = value;
        r.extra         = extra;
        r._reserved     = 0;
    }

    /// Flush all records to a binary file.
    /// Returns number of records written.
    std::size_t flush(const char* path) noexcept {
        auto h = head_.load(std::memory_order_acquire);
        auto t = tail_.load(std::memory_order_relaxed);
        if (h <= t) return 0;

        FILE* f = std::fopen(path, "ab");
        if (!f) return 0;

        std::size_t written = 0;
        while (t < h) {
            auto idx = t % CAPACITY;
            std::fwrite(&buffer_[idx], sizeof(TraceRecord), 1, f);
            ++t;
            ++written;
        }

        std::fclose(f);
        tail_.store(t, std::memory_order_release);
        return written;
    }

    /// Number of pending (unflushed) records.
    [[nodiscard]] std::size_t pending() const noexcept {
        auto h = head_.load(std::memory_order_relaxed);
        auto t = tail_.load(std::memory_order_relaxed);
        return (h >= t) ? (h - t) : 0;
    }

    /// Total records ever written.
    [[nodiscard]] std::size_t total() const noexcept {
        return head_.load(std::memory_order_relaxed);
    }

private:
    std::array<TraceRecord, CAPACITY> buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

} // namespace pg
