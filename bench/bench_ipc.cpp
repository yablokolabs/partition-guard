/// @file bench_ipc.cpp
/// Benchmark: measure IPC port latency.

#include "pg/port.hpp"
#include "pg/clock.hpp"
#include <cstdio>
#include <cstdint>

int main() {
    constexpr std::size_t ITERATIONS = 1'000'000;

    // Benchmark sampling port write+read
    {
        pg::SamplingPort<64> port;
        std::uint8_t data[64] = {0x42};
        std::uint8_t buf[64] = {};

        auto start = pg::now_ns();
        for (std::size_t i = 0; i < ITERATIONS; ++i) {
            port.write(data, 64);
            (void)port.read(buf, 64);
        }
        auto elapsed = pg::now_ns() - start;
        auto per_op_ns = elapsed / (ITERATIONS * 2); // 2 ops per iter

        std::fprintf(stderr, "SamplingPort<64>:  %zu ops in %.3fms  (%.1fns/op)\n",
                    ITERATIONS * 2, static_cast<double>(elapsed) / 1e6, static_cast<double>(per_op_ns));
    }

    // Benchmark queuing port enqueue+dequeue
    {
        pg::QueuingPort<64, 1024> port;
        std::uint8_t data[64] = {0x42};
        std::uint8_t buf[64] = {};

        auto start = pg::now_ns();
        for (std::size_t i = 0; i < ITERATIONS; ++i) {
            (void)port.enqueue(data, 64);
            (void)port.dequeue(buf, 64);
        }
        auto elapsed = pg::now_ns() - start;
        auto per_op_ns = elapsed / (ITERATIONS * 2);

        std::fprintf(stderr, "QueuingPort<64,1024>: %zu ops in %.3fms  (%.1fns/op)\n",
                    ITERATIONS * 2, static_cast<double>(elapsed) / 1e6, static_cast<double>(per_op_ns));
    }

    return 0;
}
