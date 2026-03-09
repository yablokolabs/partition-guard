#pragma once
/// @file port.hpp
/// ARINC 653-inspired IPC ports: sampling and queuing.
/// Lock-free, fixed-size, no dynamic allocation.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace pg {

/// Sampling port: latest-value semantics (overwrite on write).
/// Reader always gets the most recent value. No queue.
/// Fixed message size, specified at compile time via template.
template <std::size_t MSG_SIZE> class SamplingPort {
  public:
    SamplingPort() noexcept : seq_(0), valid_(false) {
        std::memset(buf_[0].data(), 0, MSG_SIZE);
        std::memset(buf_[1].data(), 0, MSG_SIZE);
    }

    /// Write a message (producer side). Lock-free.
    void write(const void *data, std::size_t len) noexcept {
        auto s = seq_.load(std::memory_order_relaxed);
        auto write_idx = (s + 1) & 1; // double-buffer flip
        std::size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE;
        std::memcpy(buf_[write_idx].data(), data, copy_len);
        seq_.store(s + 1, std::memory_order_release);
        valid_.store(true, std::memory_order_release);
    }

    /// Read the latest message (consumer side). Lock-free.
    /// Returns true if data was valid, false if port has never been written.
    [[nodiscard]] bool read(void *out, std::size_t len) const noexcept {
        if (!valid_.load(std::memory_order_acquire)) return false;
        auto s = seq_.load(std::memory_order_acquire);
        auto read_idx = s & 1;
        std::size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE;
        std::memcpy(out, buf_[read_idx].data(), copy_len);
        return true;
    }

    /// Check if port has valid data.
    [[nodiscard]] bool valid() const noexcept { return valid_.load(std::memory_order_acquire); }

    /// Sequence number (for freshness checking).
    [[nodiscard]] std::uint64_t sequence() const noexcept {
        return seq_.load(std::memory_order_acquire);
    }

    /// Maximum message size.
    [[nodiscard]] static constexpr std::size_t max_size() noexcept { return MSG_SIZE; }

  private:
    std::array<std::array<std::uint8_t, MSG_SIZE>, 2> buf_; // double buffer
    std::atomic<std::uint64_t> seq_;
    std::atomic<bool> valid_;
};

/// Queuing port: bounded FIFO with fixed-size messages.
/// SPSC (single producer, single consumer). Lock-free.
template <std::size_t MSG_SIZE, std::size_t DEPTH> class QueuingPort {
  public:
    QueuingPort() noexcept : head_(0), tail_(0) {}

    /// Enqueue a message. Returns false if full.
    [[nodiscard]] bool enqueue(const void *data, std::size_t len) noexcept {
        auto h = head_.load(std::memory_order_relaxed);
        auto t = tail_.load(std::memory_order_acquire);
        if (h - t >= DEPTH) return false; // full

        auto idx = h % DEPTH;
        std::size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE;
        std::memcpy(buf_[idx].data(), data, copy_len);
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    /// Dequeue a message. Returns false if empty.
    [[nodiscard]] bool dequeue(void *out, std::size_t len) noexcept {
        auto t = tail_.load(std::memory_order_relaxed);
        auto h = head_.load(std::memory_order_acquire);
        if (t >= h) return false; // empty

        auto idx = t % DEPTH;
        std::size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE;
        std::memcpy(out, buf_[idx].data(), copy_len);
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    /// Number of messages currently in queue.
    [[nodiscard]] std::size_t size() const noexcept {
        auto h = head_.load(std::memory_order_acquire);
        auto t = tail_.load(std::memory_order_acquire);
        return (h >= t) ? (h - t) : 0;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] bool full() const noexcept { return size() >= DEPTH; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return DEPTH; }
    [[nodiscard]] static constexpr std::size_t max_msg_size() noexcept { return MSG_SIZE; }

  private:
    std::array<std::array<std::uint8_t, MSG_SIZE>, DEPTH> buf_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

} // namespace pg
