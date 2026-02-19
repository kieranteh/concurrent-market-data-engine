
#include "metrics.hpp"
#include <algorithm>

using namespace std;

MetricsResult compute_metrics(const vector<uint64_t>& latencies_ns, uint64_t total_processed, double duration_sec) {
    MetricsResult result{0, 0, 0};
    result.throughput = duration_sec > 0 ? total_processed / duration_sec : 0;
    if (!latencies_ns.empty()) {
        vector<uint64_t> sorted = latencies_ns;
        
        auto it_p50 = sorted.begin() + static_cast<size_t>(0.50 * (sorted.size() - 1));
        nth_element(sorted.begin(), it_p50, sorted.end());
        result.p50_us = *it_p50 / 1000;

        auto it_p99 = sorted.begin() + static_cast<size_t>(0.99 * (sorted.size() - 1));
        // We can start from it_p50 to slightly optimize the second partition
        nth_element(it_p50, it_p99, sorted.end());
        result.p99_us = *it_p99 / 1000;
    }
    return result;
}
