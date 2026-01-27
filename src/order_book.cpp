#include "order_book.hpp"

OrderBook::OrderBook() 
    : bids_(MAX_PRICE_TICKS, 0), asks_(MAX_PRICE_TICKS, 0), 
      best_bid_idx_(-1), best_ask_idx_(MAX_PRICE_TICKS) {}

vector<Trade> OrderBook::add_order(const Event& e) {
    vector<Trade> trades;

    // Convert price to tick index (cents) for O(1) access
    // e.g. 100.00 -> 10000
    int idx = static_cast<int>(e.price * 100.0 + 0.5);
    
    if (idx < 0 || idx >= static_cast<int>(MAX_PRICE_TICKS)) return trades;

    uint32_t qty_remaining = e.quantity;

    if (e.side == Side::Buy) {
        // MATCHING ENGINE LOGIC:
        // Check if we cross the spread (Buy Price >= Best Ask)
        while (qty_remaining > 0 && best_ask_idx_ < static_cast<int>(MAX_PRICE_TICKS) && idx >= best_ask_idx_) {
            uint32_t& ask_qty = asks_[best_ask_idx_];
            uint32_t trade_qty = min(qty_remaining, ask_qty);

            trades.push_back({e.id, static_cast<double>(best_ask_idx_) / 100.0, trade_qty});

            ask_qty -= trade_qty;
            qty_remaining -= trade_qty;

            // If price level exhausted, find next best ask
            if (ask_qty == 0) {
                best_ask_idx_++;
                // Scan upwards for next valid level
                while (best_ask_idx_ < static_cast<int>(MAX_PRICE_TICKS) && asks_[best_ask_idx_] == 0) {
                    best_ask_idx_++;
                }
            }
        }

        // Post remaining quantity to book
        if (qty_remaining > 0) {
            bids_[idx] += qty_remaining;
            if (idx > best_bid_idx_) best_bid_idx_ = idx;
        }
    } else {
        // Sell Side Matching
        // Check if Sell Price <= Best Bid
        while (qty_remaining > 0 && best_bid_idx_ >= 0 && idx <= best_bid_idx_) {
            uint32_t& bid_qty = bids_[best_bid_idx_];
            uint32_t trade_qty = min(qty_remaining, bid_qty);

            trades.push_back({e.id, static_cast<double>(best_bid_idx_) / 100.0, trade_qty});

            bid_qty -= trade_qty;
            qty_remaining -= trade_qty;

            if (bid_qty == 0) {
                best_bid_idx_--;
                while (best_bid_idx_ >= 0 && bids_[best_bid_idx_] == 0) {
                    best_bid_idx_--;
                }
            }
        }

        if (qty_remaining > 0) {
            asks_[idx] += qty_remaining;
            if (idx < best_ask_idx_) best_ask_idx_ = idx;
        }
    }
    return trades;
}

pair<double, double> OrderBook::get_top_of_book() const {
    // Convert indices back to price
    double best_bid = (best_bid_idx_ == -1) ? 0.0 : (best_bid_idx_ / 100.0);
    double best_ask = (best_ask_idx_ >= static_cast<int>(MAX_PRICE_TICKS)) ? 0.0 : (best_ask_idx_ / 100.0);
    return {best_bid, best_ask};
}