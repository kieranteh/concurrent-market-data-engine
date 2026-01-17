
#include <cstddef>
#include <cstdint>
#include <ios>
#include <atomic>
#include <random>
#include <event.hpp>
#include <bounded_queue.hpp>
#include <iostream>
#include <metrics.hpp>

using namespace std;

constexpr size_t QUEUE_CAPACITY = 16; // Small queue for max stress
constexpr size_t SYMBOL_COUNT = 100;
constexpr size_t MAX_LATENCY_SAMPLES = 2'000'000;
constexpr int RUN_SECONDS = 10;

atomic<bool> stop_flag{false};
atomic<uint64_t> total_produced{0};
atomic<uint64_t> total_processed{0};

void producer(BoundedQueue<Event>& queue) {
    mt19937 rng{random_device{}()};
    uniform_int_distribution<uint32_t> symbol_dist(0, SYMBOL_COUNT - 1);
    uniform_real_distribution<double> price_dist(100.0, 200.0);
    uint64_t id = 0;
    while (!stop_flag.load(memory_order_relaxed)) {
        Event e;
        e.id = id++;
        e.t0_ns = now_ns();
        e.symbol_id = symbol_dist(rng);
        e.price = price_dist(rng);
        if (!queue.push(e)) break;
        ++total_produced;
    }
}

void consumer(BoundedQueue<Event>& queue, vector<uint64_t>& latencies, vector<double>& latest_prices) {
    Event e;
    while (queue.pop(e)) {
        uint64_t latency = now_ns() - e.t0_ns;
        if (latencies.size() < MAX_LATENCY_SAMPLES) {
            latencies.push_back(latency);
        }
        latest_prices[e.symbol_id] = e.price;
        ++total_processed;
    }
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
    BoundedQueue<Event> queue(QUEUE_CAPACITY);
    vector<uint64_t> latencies;
    latencies.reserve(MAX_LATENCY_SAMPLES);
    vector<double> latest_prices(SYMBOL_COUNT, 0.0);

    thread prod(producer, ref(queue));
    thread cons(consumer, ref(queue), ref(latencies), ref(latest_prices));
    atomic<bool> stats_stop{false};
    thread stats(stats_printer, ref(stats_stop), ref(total_processed));

    auto start = chrono::steady_clock::now();
    this_thread::sleep_for(chrono::seconds(RUN_SECONDS));
    stop_flag = true;
    queue.close();
    prod.join();
    cons.join();
    stats_stop = true;
    stats.join();
    auto end = chrono::steady_clock::now();
    double duration = chrono::duration<double>(end - start).count();

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
