/// @file test_port.cpp
/// Tests for IPC ports (sampling and queuing).

#include "pg/port.hpp"
#include <cstdio>
#include <cassert>
#include <cstring>

void test_sampling_port_basic() {
    pg::SamplingPort<64> port;
    assert(!port.valid() && "Fresh port should be invalid");

    const char msg[] = "hello partition";
    port.write(msg, sizeof(msg));
    assert(port.valid() && "Port should be valid after write");
    assert(port.sequence() == 1);

    char buf[64] = {};
    bool ok = port.read(buf, sizeof(buf));
    assert(ok && "Read should succeed");
    assert(std::strcmp(buf, "hello partition") == 0);
    (void)ok;
    (void)buf;

    std::fprintf(stderr, "  ✓ sampling port basic\n");
}

void test_sampling_port_overwrite() {
    pg::SamplingPort<32> port;

    const char msg1[] = "first";
    const char msg2[] = "second";

    port.write(msg1, sizeof(msg1));
    port.write(msg2, sizeof(msg2));

    char buf[32] = {};
    bool ok = port.read(buf, sizeof(buf));
    assert(ok);
    assert(std::strcmp(buf, "second") == 0);
    assert(port.sequence() == 2);
    (void)ok;

    std::fprintf(stderr, "  ✓ sampling port overwrite\n");
}

void test_queuing_port_basic() {
    pg::QueuingPort<32, 4> port;
    assert(port.empty());
    assert(port.size() == 0);

    assert(port.enqueue("one", 4));
    assert(port.enqueue("two", 4));
    assert(port.enqueue("three", 6));
    assert(port.size() == 3);

    char buf[32] = {};
    bool ok;

    ok = port.dequeue(buf, sizeof(buf));
    assert(ok && std::strcmp(buf, "one") == 0);

    ok = port.dequeue(buf, sizeof(buf));
    assert(ok && std::strcmp(buf, "two") == 0);

    ok = port.dequeue(buf, sizeof(buf));
    assert(ok && std::strcmp(buf, "three") == 0);
    (void)ok;

    assert(port.empty());

    std::fprintf(stderr, "  ✓ queuing port basic (FIFO)\n");
}

void test_queuing_port_full() {
    pg::QueuingPort<16, 2> port;

    assert(port.enqueue("a", 2));
    assert(port.enqueue("b", 2));
    assert(!port.enqueue("c", 2) && "Should fail when full");
    assert(port.full());

    std::fprintf(stderr, "  ✓ queuing port full detection\n");
}

void test_queuing_port_empty() {
    pg::QueuingPort<16, 4> port;
    char buf[16] = {};
    bool ok = port.dequeue(buf, sizeof(buf));
    assert(!ok && "Should fail when empty");
    assert(port.empty());
    (void)ok;
    (void)buf;

    std::fprintf(stderr, "  ✓ queuing port empty detection\n");
}

int main() {
    std::fprintf(stderr, "test_port:\n");
    test_sampling_port_basic();
    test_sampling_port_overwrite();
    test_queuing_port_basic();
    test_queuing_port_full();
    test_queuing_port_empty();
    std::fprintf(stderr, "  All port tests passed.\n");
    return 0;
}
