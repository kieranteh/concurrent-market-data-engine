#include <cstddef>
#include <cstdint>
#include <ios>
#include <atomic>
#include <random>
#include <vector>
#include <event.hpp>
#include <bounded_queue.hpp>
#include <iostream>
#include <metrics.hpp>
#include <iomanip>
#include <memory>
#include <order_book.hpp>
#include <cmath>

using namespace std;

using EventQueue = BoundedQueue<Event>;

constexpr size_t QUEUE_CAPACITY = 1024; // Smaller queue to reduce latency (Buffer Bloat)
constexpr size_t SYMBOL_COUNT = 100;
constexpr size_t MAX_LATENCY_SAMPLES = 2'000'000;
constexpr int RUN_SECONDS = 10;
constexpr int NUM_PRODUCERS = 4; // Reduced to prevent CPU oversubscription
constexpr int NUM_CONSUMERS = 4; // Reduced to prevent CPU oversubscription

// Align atomics to cache lines (64 bytes) to prevent false sharing
alignas(64) atomic<bool> stop_flag{false};
alignas(64) atomic<uint64_t> total_produced{0};
alignas(64) atomic<uint64_t> total_processed{0};


void producer(int producer_id, vector<unique_ptr<EventQueue>>& queues) {
    mt19937 rng{random_device{}()};
    uniform_int_distribution<uint32_t> symbol_dist(0, SYMBOL_COUNT - 1);
    uniform_real_distribution<double> price_dist(100.0, 200.0);
    uniform_int_distribution<uint32_t> qty_dist(1, 1000);
    uniform_int_distribution<int> side_dist(0, 1);

    uint64_t id = 0;
    uint64_t local_produced_count = 0;

    while (!stop_flag.load(memory_order_relaxed)) {
        Event e;
        e.id = id++;
        e.t0_ns = now_ns();
        e.symbol_id = symbol_dist(rng);
        // OPTIMISATION: Round price to 2 decimal places (cents).
        // This prevents the OrderBook map from growing infinitely with unique doubles,
        // simulating real market ticks and keeping cache performance high.
        e.price = round(price_dist(rng) * 100.0) / 100.0;
        e.quantity = qty_dist(rng);
        e.side = static_cast<Side>(side_dist(rng));

        // SHARDING: Route by symbol_id to ensure ordering.
        // All events for "AAPL" must go to the same queue.
        size_t queue_idx = e.symbol_id % queues.size();
        
        if (!queues[queue_idx]->push(e)) break;
        
        // Batch atomic updates to reduce bus contention
        if (++local_produced_count % 1024 == 0) total_produced.fetch_add(1024, memory_order_relaxed);
    }
    total_produced.fetch_add(local_produced_count % 1024, memory_order_relaxed);
}

void consumer(EventQueue& queue, vector<uint64_t>& latencies) {
    const size_t max_samples = MAX_LATENCY_SAMPLES / NUM_CONSUMERS;
    
    // Each consumer maintains the state for the symbols it is responsible for.
    // No locks needed here because of symbol sharding!
    vector<OrderBook> books(SYMBOL_COUNT);

    Event e;
    uint64_t local_processed_count = 0;
    while (queue.pop(e)) {
        if (latencies.size() < max_samples) {
            uint64_t latency = now_ns() - e.t0_ns;
            latencies.push_back(latency);
        }
        
        // Process business logic
        books[e.symbol_id].add_order(e);

        // Batch atomic updates
        if (++local_processed_count % 1024 == 0) total_processed.fetch_add(1024, memory_order_relaxed);
    }
    total_processed.fetch_add(local_processed_count % 1024, memory_order_relaxed);
}

void stats_printer(atomic<bool>& stop_flag, atomic<uint64_t>& total_processed) {
    uint64_t last_processed = 0;
    while (!stop_flag.load()) {
        this_thread::sleep_for(chrono::seconds(1));
        uint64_t now = total_processed.load();
        cout << "[Stats] Processed: " << now
             << " (+" << (now - last_processed) << "/s)" << endl;
        last_processed = now;
    }
}

int main() {
    vector<unique_ptr<EventQueue>> queues;
    for(int i = 0; i < NUM_CONSUMERS; ++i) {
        queues.push_back(make_unique<EventQueue>(QUEUE_CAPACITY));
    }
    
    vector<vector<uint64_t>> consumer_latencies(NUM_CONSUMERS);
    for(auto& v : consumer_latencies) v.reserve(MAX_LATENCY_SAMPLES / NUM_CONSUMERS);

    vector<thread> producers;
    vector<thread> consumers;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        producers.emplace_back(producer, i, ref(queues));
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        consumers.emplace_back(consumer, ref(*queues[i]), ref(consumer_latencies[i]));

    atomic<bool> stats_stop{false};
    thread stats(stats_printer, ref(stats_stop), ref(total_processed));

    auto start = chrono::steady_clock::now();
    this_thread::sleep_for(chrono::seconds(RUN_SECONDS));
    stop_flag = true;
    for(auto& q : queues) q->close();
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    stats_stop = true;
    stats.join();
    auto end = chrono::steady_clock::now();
    double duration = chrono::duration<double>(end - start).count();

    vector<uint64_t> latencies;
    latencies.reserve(MAX_LATENCY_SAMPLES);
    for(const auto& v : consumer_latencies) {
        latencies.insert(latencies.end(), v.begin(), v.end());
    }

    MetricsResult metrics = compute_metrics(latencies, total_processed, duration);

    cout << fixed << setprecision(2);
    cout << "Duration: " << duration << " s\n";
    cout << "Produced: " << total_produced << ", Processed: " << total_processed << "\n";
    cout << "Throughput: " << metrics.throughput << " events/sec\n";
    cout << "p50 latency: " << metrics.p50_us << " us\n";
    cout << "p99 latency: " << metrics.p99_us << " us\n";
    cout << "Latency samples: " << latencies.size() << "\n";
    return 0;
}
