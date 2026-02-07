#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

using namespace std;

struct MetricsResult {
    double throughput;
    uint64_t p50_us;
    uint64_t p99_us;
};

MetricsResult compute_metrics(const vector<uint64_t>& latencies_ns, uint64_t total_processed, double duration_sec);
