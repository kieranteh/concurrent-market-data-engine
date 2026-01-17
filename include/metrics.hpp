#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

struct MetricsResult {
    double throughput;
    uint64_t p50_us;
    uint64_t p99_us;
};

MetricsResult compute_metrics(const std::vector<uint64_t>& latencies_ns, uint64_t total_processed, double duration_sec);
