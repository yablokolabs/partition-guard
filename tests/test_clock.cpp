/// @file test_clock.cpp
/// Tests for clock utilities.

#include "pg/clock.hpp"
#include <cassert>
#include <cstdio>

void test_monotonicity() {
    auto t1 = pg::now_ns();
    auto t2 = pg::now_ns();
    assert(t2 >= t1 && "Clock must be monotonic");
    std::fprintf(stderr, "  ✓ monotonicity (delta=%ldns)\n", t2 - t1);
}

void test_sleep_accuracy() {
    auto start = pg::now_ns();
    auto target = start + pg::ms_to_ns(10); // sleep 10ms
    auto actual = pg::sleep_until_ns(target);
    auto jitter = actual - target;

    // Allow up to 1ms of jitter (generous for non-RT kernel)
    assert(jitter < pg::ms_to_ns(1) && "Sleep jitter too high");
    assert(actual >= target && "Woke up too early");
    std::fprintf(stderr, "  ✓ sleep accuracy (jitter=%ldμs)\n", jitter / 1000);
}

void test_conversions() {
    assert(pg::ms_to_ns(1) == 1'000'000);
    assert(pg::us_to_ns(1) == 1'000);
    assert(pg::ms_to_ns(0) == 0);
    std::fprintf(stderr, "  ✓ conversions\n");
}

int main() {
    std::fprintf(stderr, "test_clock:\n");
    test_monotonicity();
    test_sleep_accuracy();
    test_conversions();
    std::fprintf(stderr, "  All clock tests passed.\n");
    return 0;
}
