
#include "metrics.hpp"
#include <algorithm>

using namespace std;

MetricsResult compute_metrics(const vector<uint64_t>& latencies_ns, uint64_t total_processed, double duration_sec) {
    MetricsResult result{0, 0, 0};
    result.throughput = duration_sec > 0 ? total_processed / duration_sec : 0;
    if (!latencies_ns.empty()) {
        vector<uint64_t> sorted = latencies_ns;
        sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        size_t p50_idx = static_cast<size_t>(0.50 * (n - 1));
        size_t p99_idx = static_cast<size_t>(0.99 * (n - 1));
        result.p50_us = sorted[p50_idx] / 1000;
        result.p99_us = sorted[p99_idx] / 1000;
    }
    return result;
}
