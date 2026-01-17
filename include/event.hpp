#pragma once
#include <cstdint>
#include <chrono>

using namespace std;

struct Event {
    uint64_t id;
    uint64_t t0_ns;
    uint32_t symbol_id;
    double price;
};

inline uint64_t now_ns() {
    return chrono::duration_cast<std::chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
}
