#pragma once
#include "event.hpp"
#include <vector>
#include <utility>

// Represents a simplified Limit Order Book (LOB)
// Aggregates quantity at each price level.
class OrderBook {
public:
    OrderBook();
    void add_order(const Event& e);
    
    // Returns the Best Bid and Best Ask prices
    // Returns {0.0, 0.0} if side is empty
    std::pair<double, double> get_top_of_book() const;

private:
    // Optimization: Use flat arrays for O(1) access instead of std::map O(log N).
    // Index = price * 100. Max price 250.00 covers the simulation range.
    static constexpr size_t MAX_PRICE_TICKS = 25000;

    std::vector<uint32_t> bids_;
    std::vector<uint32_t> asks_;

    int best_bid_idx_;
    int best_ask_idx_;
};