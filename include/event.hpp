#pragma once
#include <cstdint>
#include <chrono>

using namespace std;

enum class Side : uint8_t { Buy, Sell };

struct Event {
    uint64_t id;
    uint64_t t0_ns;
    uint32_t symbol_id;
    double price;
    uint32_t quantity;
    Side side;
};

inline uint64_t now_ns() {
    return chrono::duration_cast<std::chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
}
