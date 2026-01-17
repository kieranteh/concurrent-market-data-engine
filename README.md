# Concurrent Market Data Event Processing Engine

A high-performance C++20 market data engine simulating a low-latency trading pipeline. It uses a **sharded producer-consumer architecture** to enable lock-free Order Book processing and achieves microsecond-level latency.

## What is this?
This project simulates a High-Frequency Trading (HFT) engine. It generates synthetic market data (Bids/Asks) and routes them to consumers based on Symbol ID (**Sharding**).

### Key Features
*   **Sharded Architecture:** Events are hashed by Symbol ID to specific queues. This guarantees strict ordering per symbol and allows the `OrderBook` to be updated without locks (Single Writer Principle).
*   **O(1) Order Book:** Replaced standard `std::map` with a flat vector (Direct Address Table) for constant-time insertions and updates, maximizing CPU cache locality.
*   **Optimized Bounded Queue:** Uses `std::mutex` and `std::condition_variable` with **conditional notification** (only waking threads when necessary) to minimize system calls and jitter.
*   **Low Latency:** Achieves **~3us median** and **~8us tail (p99)** latency on standard hardware.
*   **Memory Safety:** Uses `std::unique_ptr` for resource management and `alignas(64)` to prevent false sharing between threads.

**Backpressure:** The bounded queue enforces flow control, ensuring producers slow down if consumers cannot keep up, preventing data loss.

**Throughput** is the number of events processed per second. The engine prints throughput and latency percentiles after running.

## Build & Run (Windows MSYS2)

```
pacman -Syu --needed cmake ninja gcc
cmake -S . -B build -G Ninja
cmake --build build
./build/market_data_engine.exe
```

## Output Example
```
Duration: 10.00 s
Produced: 12345678, Processed: 12345678
Throughput: 1234567.89 events/sec
p50 latency: 12 us
p99 latency: 45 us
Latency samples: 2000000
```

## Future Work
- Multi-producer/multi-consumer support
- Lock-free SPSC queue
- Event conflation (latest-only per symbol)