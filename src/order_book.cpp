#include "order_book.hpp"

OrderBook::OrderBook() 
    : bids_(MAX_PRICE_TICKS, 0), asks_(MAX_PRICE_TICKS, 0), 
      best_bid_idx_(-1), best_ask_idx_(MAX_PRICE_TICKS) {}

void OrderBook::add_order(const Event& e) {
    // Convert price to tick index (cents) for O(1) access
    // e.g. 100.00 -> 10000
    int idx = static_cast<int>(e.price * 100.0 + 0.5);
    
    if (idx < 0 || idx >= static_cast<int>(MAX_PRICE_TICKS)) return;

    if (e.side == Side::Buy) {
        bids_[idx] += e.quantity;
        // Track best bid (highest index)
        if (idx > best_bid_idx_) best_bid_idx_ = idx;
    } else {
        asks_[idx] += e.quantity;
        // Track best ask (lowest index)
        if (idx < best_ask_idx_) best_ask_idx_ = idx;
    }
}

pair<double, double> OrderBook::get_top_of_book() const {
    // Convert indices back to price
    double best_bid = (best_bid_idx_ == -1) ? 0.0 : (best_bid_idx_ / 100.0);
    double best_ask = (best_ask_idx_ >= static_cast<int>(MAX_PRICE_TICKS)) ? 0.0 : (best_ask_idx_ / 100.0);
    return {best_bid, best_ask};
}