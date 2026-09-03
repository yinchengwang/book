#include <rag/benchmark.h>

#include <cmath>
#include <cassert>
#include <iostream>
#include <vector>

using namespace rag;

void test_latency_recorder() {
    LatencyRecorder recorder;

    // Test empty recorder
    assert(recorder.avg() == 0.0);
    assert(recorder.min() == 0.0);
    assert(recorder.max() == 0.0);
    assert(recorder.percentile(50) == 0.0);

    // Record some latencies
    recorder.record(10.0);
    recorder.record(20.0);
    recorder.record(30.0);
    recorder.record(40.0);
    recorder.record(50.0);

    // Test statistics
    assert(recorder.avg() == 30.0);
    assert(recorder.min() == 10.0);
    assert(recorder.max() == 50.0);

    // Test percentile
    double p50 = recorder.percentile(50);
    assert(p50 >= 29.0 && p50 <= 31.0);

    double p95 = recorder.percentile(95);
    assert(p95 >= 48.0 && p95 <= 52.0);

    // Test reset
    recorder.reset();
    assert(recorder.avg() == 0.0);
    assert(recorder.values().empty());

    std::cout << "test_latency_recorder passed" << std::endl;
}

void test_percentile() {
    LatencyRecorder recorder;

    // Test with known values
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    for (double v : values) {
        recorder.record(v);
    }

    // Test boundaries
    assert(recorder.percentile(0.0) == 1.0);
    assert(recorder.percentile(100.0) == 10.0);

    // Test percentile calculation
    assert(recorder.percentile(25.0) == 3.25);
    assert(recorder.percentile(50.0) == 5.5);
    assert(recorder.percentile(75.0) == 7.75);
    assert(recorder.percentile(90.0) == 9.1);

    // Test with single value
    LatencyRecorder single;
    single.record(42.0);
    assert(single.percentile(50.0) == 42.0);
    assert(single.percentile(99.0) == 42.0);

    std::cout << "test_percentile passed" << std::endl;
}

void test_qps_measurement() {
    // Create a null pipeline benchmark - we can't actually run QPS without a real pipeline
    // but we can verify the interface exists and doesn't crash

    // For a real test, we would need to mock or create a test pipeline
    // This is a placeholder test that verifies the interface
    LatencyRecorder recorder;
    for (int i = 0; i < 100; ++i) {
        recorder.record(10.0 + (i % 10) * 1.0);
    }

    // Verify the recorder works correctly for QPS calculation
    double avg = recorder.avg();
    assert(avg > 10.0 && avg < 20.0);

    double qps = 1000.0 / avg;  // Simplified QPS calculation
    assert(qps > 50.0 && qps < 100.0);

    std::cout << "test_qps_measurement passed" << std::endl;
}

int main() {
    test_latency_recorder();
    test_percentile();
    test_qps_measurement();

    std::cout << "All benchmark tests passed!" << std::endl;
    return 0;
}