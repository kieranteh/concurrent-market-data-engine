#include "../include/bounded_queue.hpp"
#include "../include/event.hpp"
#include "../include/metrics.hpp"
#include "../include/order_book.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std;

using EventQueue = BoundedQueue<Event>;

constexpr size_t QUEUE_CAPACITY =
    1024; // Smaller queue to reduce latency (Buffer Bloat)
constexpr size_t SYMBOL_COUNT = 100;
constexpr size_t MAX_LATENCY_SAMPLES = 2'000'000;
constexpr int RUN_SECONDS = 600; // Run for 10 minutes for the web demo
constexpr int NUM_PRODUCERS = 4; // Reduced to prevent CPU oversubscription
constexpr int NUM_CONSUMERS = 4; // Reduced to prevent CPU oversubscription

const std::vector<std::string> STOCK_SYMBOLS = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "META",  "TSLA", "NVDA", "BRK.B", "JPM",
    "JNJ",  "V",    "UNH",   "PG",   "HD",    "DIS",  "MA",   "PYPL",  "BAC",
    "VZ",   "ADBE", "CRM",   "NFLX", "CMCSA", "XOM",  "PFE",  "NKE",   "INTC",
    "T",    "KO",   "MRK",   "PEP",  "ABT",   "WMT",  "CVX",  "CSCO",  "MCD",
    "ABBV", "MDT",  "BMY",   "ACN",  "AVGO",  "TXN",  "COST", "NEE",   "QCOM",
    "DHR",  "LIN",  "PM",    "UNP",  "LOW",   "HON",  "UPS",  "ORCL",  "IBM",
    "SBUX", "MMM",  "GE",    "CAT",  "BA",    "GS",   "C",    "WFC",   "USB",
    "MS",   "RTX",  "LMT",   "BLK",  "DE",    "ISRG", "TMO",  "AMGN",  "GILD",
    "CVS",  "MO",   "AXP",   "SPGI", "SYK",   "CI",   "ELV",  "PLD",   "ZTS",
    "ADP",  "MDLZ", "GPN",   "CCI",  "TGT",   "TJX",  "BDX",  "CL",    "DUK",
    "SO",   "D",    "SLB",   "EOG",  "COP",   "MMC",  "ITW",  "NSC",   "APD",
    "FDX"};

// Align atomics to cache lines (64 bytes) to prevent false sharing
alignas(64) atomic<bool> stop_flag{false};
alignas(64) atomic<uint64_t> total_produced{0};
alignas(64) atomic<uint64_t> total_processed{0};
alignas(64) atomic<uint64_t> total_trades{0};

void producer(int producer_id, vector<unique_ptr<EventQueue>> &queues) {
  mt19937 rng{random_device{}()};
  uniform_int_distribution<uint32_t> symbol_dist(0, SYMBOL_COUNT - 1);
  uniform_real_distribution<double> price_dist(100.0, 200.0);
  uniform_int_distribution<uint32_t> qty_dist(1, 1000);
  uniform_int_distribution<int> side_dist(0, 1);

  uint64_t id = 0;
  uint64_t local_produced_count = 0;

  constexpr size_t BATCH_SIZE = 64;
  vector<vector<Event>> local_buffers(queues.size());
  for (auto &buf : local_buffers) {
    buf.reserve(BATCH_SIZE);
  }

  while (!stop_flag.load(memory_order_relaxed)) {
    Event e;
    e.id = id++;
    e.t0_ns = now_ns();
    e.symbol_id = symbol_dist(rng);
    // OPTIMISATION: Round price to 2 decimal places (cents).
    // This prevents the OrderBook map from growing infinitely with unique
    // doubles, simulating real market ticks and keeping cache performance high.
    e.price = round(price_dist(rng) * 100.0) / 100.0;
    e.quantity = qty_dist(rng);
    e.side = static_cast<Side>(side_dist(rng));

    // SHARDING: Route by symbol_id to ensure ordering.
    // All events for "AAPL" must go to the same queue.
    size_t queue_idx = e.symbol_id % queues.size();

    // Append to thread-local buffer
    local_buffers[queue_idx].push_back(e);

    // Flush batch when buffer is full
    if (local_buffers[queue_idx].size() == BATCH_SIZE) {
      if (!queues[queue_idx]->push_batch(local_buffers[queue_idx]))
        break;
      local_buffers[queue_idx].clear();
    }

    // Batch atomic updates to reduce bus contention
    if (++local_produced_count % 1024 == 0)
      total_produced.fetch_add(1024, memory_order_relaxed);
  }

  // Flush any remaining buffered events on thread exit
  for (size_t i = 0; i < local_buffers.size(); ++i) {
    if (!local_buffers[i].empty()) {
      // It's safe if push_batch fails here because we are stopping anyway
      queues[i]->push_batch(local_buffers[i]);
      local_buffers[i].clear();
    }
  }

  total_produced.fetch_add(local_produced_count % 1024, memory_order_relaxed);
}

void consumer(EventQueue &queue, vector<uint64_t> &latencies) {
  const size_t max_samples = MAX_LATENCY_SAMPLES / NUM_CONSUMERS;

  // Each consumer maintains the state for the symbols it is responsible for.
  // No locks needed here because of symbol sharding!
  vector<OrderBook> books(SYMBOL_COUNT);

  Event e;
  uint64_t local_processed_count = 0;
  uint64_t local_trade_count = 0;

  while (queue.pop(e)) {
    if (latencies.size() < max_samples) {
      uint64_t latency = now_ns() - e.t0_ns;
      latencies.push_back(latency);
    }

    // Process business logic
    vector<Trade> trades = books[e.symbol_id].add_order(e);
    local_trade_count += trades.size();

    for (const auto &t : trades) {
      // Log significant trades for the frontend (throttle to avoid IO
      // bottleneck)
      if (t.quantity > 500) {
        // Use cout for consistency and to avoid mixing I/O streams
        cout << "{\"symbol\": \""
             << STOCK_SYMBOLS[e.symbol_id % STOCK_SYMBOLS.size()]
             << "\", \"side\": \"" << (e.side == Side::Buy ? 'B' : 'S')
             << "\", \"price\": " << fixed << setprecision(2) << t.price
             << ", \"quantity\": " << t.quantity << "}\n";
      }
    }

    // Batch atomic updates
    if (++local_processed_count % 1024 == 0) {
      total_processed.fetch_add(1024, memory_order_relaxed);
      total_trades.fetch_add(local_trade_count, memory_order_relaxed);
      local_trade_count = 0;
    }
  }
  total_processed.fetch_add(local_processed_count % 1024, memory_order_relaxed);
  total_trades.fetch_add(local_trade_count, memory_order_relaxed);
}

void stats_printer(atomic<bool> &stop_flag, atomic<uint64_t> &total_processed,
                   atomic<uint64_t> &total_trades) {
  auto last_time = chrono::steady_clock::now();
  uint64_t last_processed = total_processed.load(memory_order_relaxed);

  while (!stop_flag.load()) {
    this_thread::sleep_for(
        chrono::milliseconds(200)); // Update 5 times a second for smooth UI

    auto now_time = chrono::steady_clock::now();
    uint64_t current_processed = total_processed.load(memory_order_relaxed);
    uint64_t current_trades = total_trades.load(memory_order_relaxed);

    double duration_sec =
        chrono::duration<double>(now_time - last_time).count();
    uint64_t processed_diff = current_processed - last_processed;
    uint64_t throughput =
        (duration_sec > 0)
            ? static_cast<uint64_t>(processed_diff / duration_sec)
            : 0;

    // Output JSON for the Node.js backend to parse
    cout << "{\"processed\": " << current_processed
         << ", \"trades\": " << current_trades
         << ", \"throughput\": " << throughput << "}" << endl;
    last_processed = current_processed;
    last_time = now_time;
  }
}

int main() {
  vector<unique_ptr<EventQueue>> queues;
  for (int i = 0; i < NUM_CONSUMERS; ++i) {
    queues.push_back(make_unique<EventQueue>(QUEUE_CAPACITY));
  }

  vector<vector<uint64_t>> consumer_latencies(NUM_CONSUMERS);
  for (auto &v : consumer_latencies)
    v.reserve(MAX_LATENCY_SAMPLES / NUM_CONSUMERS);

  vector<thread> producers;
  vector<thread> consumers;
  for (int i = 0; i < NUM_PRODUCERS; ++i)
    producers.emplace_back(producer, i, ref(queues));
  for (int i = 0; i < NUM_CONSUMERS; ++i)
    consumers.emplace_back(consumer, ref(*queues[i]),
                           ref(consumer_latencies[i]));

  atomic<bool> stats_stop{false};
  thread stats(stats_printer, ref(stats_stop), ref(total_processed),
               ref(total_trades));

  auto start = chrono::steady_clock::now();
  this_thread::sleep_for(chrono::seconds(RUN_SECONDS));
  stop_flag = true;
  for (auto &q : queues)
    q->close();
  for (auto &t : producers)
    t.join();
  for (auto &t : consumers)
    t.join();
  stats_stop = true;
  stats.join();
  auto end = chrono::steady_clock::now();
  double duration = chrono::duration<double>(end - start).count();

  vector<uint64_t> latencies;
  latencies.reserve(MAX_LATENCY_SAMPLES);
  for (const auto &v : consumer_latencies) {
    latencies.insert(latencies.end(), v.begin(), v.end());
  }

  MetricsResult metrics = compute_metrics(latencies, total_processed, duration);

  cout << "\n--- Final Results ---\n" << fixed << setprecision(2);
  cout << "Duration: " << duration << " s\n";
  cout << "Produced: " << total_produced << ", Processed: " << total_processed
       << "\n";
  cout << "Trades Executed: " << total_trades << "\n";
  cout << "Throughput: " << metrics.throughput << " events/sec\n";
  cout << "p50 latency: " << metrics.p50_us << " us\n";
  cout << "p99 latency: " << metrics.p99_us << " us\n";
  cout << "Latency samples: " << latencies.size() << "\n";
  return 0;
}
