/// @file test_health.cpp
/// Tests for the health monitor.

#include "pg/health_monitor.hpp"
#include <cassert>
#include <cstdio>

void test_initial_state() {
    pg::HealthMonitor hm;
    assert(hm.status(0).overrun_count == 0);
    assert(hm.status(0).restart_count == 0);
    assert(!hm.status(0).faulted);
    std::fprintf(stderr, "  ✓ initial state clean\n");
}

void test_single_overrun_warns() {
    pg::HealthMonitor hm;
    assert(hm.report_overrun(0, 1000, 100000, false) == pg::HealthAction::LogWarning);
    assert(hm.status(0).overrun_count == 1);
    std::fprintf(stderr, "  ✓ single overrun → warning\n");
}

void test_consecutive_overruns_restart() {
    pg::HealthMonitor hm;

    // 3 consecutive overruns should trigger restart
    (void)hm.report_overrun(0, 1000, 100000, false);
    (void)hm.report_overrun(0, 1000, 200000, false);
    assert(hm.report_overrun(0, 1000, 300000, false) == pg::HealthAction::RestartPartition);
    assert(hm.status(0).restart_count == 1);
    std::fprintf(stderr, "  ✓ 3 consecutive overruns → restart\n");
}

void test_success_resets_counter() {
    pg::HealthMonitor hm;

    (void)hm.report_overrun(0, 1000, 100000, false);
    (void)hm.report_overrun(0, 1000, 200000, false);
    hm.report_success(0); // Reset consecutive counter
    (void)hm.report_overrun(0, 1000, 300000, false);

    // Only 2 consecutive after reset, should be warning not restart
    assert(hm.report_overrun(0, 1000, 400000, false) == pg::HealthAction::LogWarning);
    std::fprintf(stderr, "  ✓ success resets consecutive counter\n");
}

void test_critical_partition_halts() {
    pg::HealthMonitor hm;

    // Exhaust restarts on a critical partition
    for (int r = 0; r < 5; ++r) {
        for (int i = 0; i < 3; ++i) {
            (void)hm.report_overrun(0, 1000, static_cast<std::int64_t>(r * 100000 + i * 1000),
                                    true);
        }
    }

    // Next threshold breach should halt
    (void)hm.report_overrun(0, 1000, 999000, true);
    (void)hm.report_overrun(0, 1000, 999100, true);
    assert(hm.report_overrun(0, 1000, 999200, true) == pg::HealthAction::HaltSystem);
    assert(hm.status(0).faulted);
    std::fprintf(stderr, "  ✓ critical partition → halt after max restarts\n");
}

void test_worst_overrun_tracking() {
    pg::HealthMonitor hm;
    (void)hm.report_overrun(0, 500, 100000, false);
    (void)hm.report_overrun(0, 2000, 200000, false);
    (void)hm.report_overrun(0, 1000, 300000, false);

    assert(hm.status(0).worst_overrun_ns == 2000);
    std::fprintf(stderr, "  ✓ worst overrun tracking\n");
}

int main() {
    std::fprintf(stderr, "test_health:\n");
    test_initial_state();
    test_single_overrun_warns();
    test_consecutive_overruns_restart();
    test_success_resets_counter();
    test_critical_partition_halts();
    test_worst_overrun_tracking();
    std::fprintf(stderr, "  All health tests passed.\n");
    return 0;
}
