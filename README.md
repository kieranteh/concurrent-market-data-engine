# Concurrent Market Data Event Processing Engine

A simple C++20 producer-consumer engine for learning about mutexes and condition variables. It simulates a market data pipeline with a bounded, thread-safe queue and measures throughput and latency.

## What is this?
This project demonstrates a concurrent event processing pipeline: one producer thread generates synthetic market data events, and one consumer thread processes them. The queue uses a ring buffer, `std::mutex`, and `std::condition_variable` to provide backpressure and safe communication between threads.

**Backpressure** means the producer will block (wait) if the queue is full, and the consumer will block if the queue is empty. This ensures no events are lost and the system adapts to the slowest stage.

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
# CPP-Quant-Project