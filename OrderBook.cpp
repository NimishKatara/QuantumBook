// OrderBook.cpp
#include "OrderBook.h"
#include <stdexcept>

void OrderBook::add_order(Order order) {
    if (order.type == OrderType::MARKET) {
        match_market(order);
        // market orders never rest on the book
        return;
    }
    // limit order: try to match first, then rest any remainder
    match_limit(order);

    if (order.remaining > 0) {
        if (order.side == Side::BUY)
            bids_[order.price].push_back(order);
        else
            asks_[order.price].push_back(order);
    }
}




void OrderBook::match_limit(Order& incoming) {
    if (incoming.side == Side::BUY) {
        // sweep asks from lowest price upward, stop when no more crosses
        while (incoming.remaining > 0 && !asks_.empty()) {
            auto it = asks_.begin();           // best ask (lowest)
            if (it->first > incoming.price)    // no cross — done
                break;

            auto& queue = it->second;
            while (incoming.remaining > 0 && !queue.empty()) {
                Order& resting = queue.front(); // time priority: FIFO

                uint32_t fill = std::min(incoming.remaining, resting.remaining);

                Trade t{ incoming.order_id, resting.order_id,
                         resting.price,    // fill at resting order's price
                         fill };
                on_trade_(t);

                incoming.remaining -= fill;
                resting.remaining  -= fill;

                if (resting.remaining == 0)
                    queue.pop_front();
            }
            if (queue.empty())
                asks_.erase(it);    // price level exhausted — remove it
        }
    } else {
        // SELL: sweep bids from highest price downward
        while (incoming.remaining > 0 && !bids_.empty()) {
            auto it = bids_.begin();           // best bid (highest)
            if (it->first < incoming.price)    // no cross — done
                break;

            auto& queue = it->second;
            while (incoming.remaining > 0 && !queue.empty()) {
                Order& resting = queue.front();

                uint32_t fill = std::min(incoming.remaining, resting.remaining);

                Trade t{ resting.order_id, incoming.order_id,
                         resting.price,
                         fill };
                on_trade_(t);

                incoming.remaining -= fill;
                resting.remaining  -= fill;

                if (resting.remaining == 0)
                    queue.pop_front();
            }
            if (queue.empty())
                bids_.erase(it);
        }
    }
}





void OrderBook::match_market(Order& incoming) {
    // market orders are identical to limit match but with no price check —
    // they sweep until filled or the book is empty
    if (incoming.side == Side::BUY) {
        while (incoming.remaining > 0 && !asks_.empty()) {
            auto it    = asks_.begin();
            auto& queue = it->second;

            while (incoming.remaining > 0 && !queue.empty()) {
                Order& resting = queue.front();
                uint32_t fill  = std::min(incoming.remaining, resting.remaining);

                on_trade_({ incoming.order_id, resting.order_id,
                             resting.price, fill });

                incoming.remaining -= fill;
                resting.remaining  -= fill;
                if (resting.remaining == 0) queue.pop_front();
            }
            if (queue.empty()) asks_.erase(it);
        }
    } else {
        while (incoming.remaining > 0 && !bids_.empty()) {
            auto it    = bids_.begin();
            auto& queue = it->second;

            while (incoming.remaining > 0 && !queue.empty()) {
                Order& resting = queue.front();
                uint32_t fill  = std::min(incoming.remaining, resting.remaining);

                on_trade_({ resting.order_id, incoming.order_id,
                             resting.price, fill });

                incoming.remaining -= fill;
                resting.remaining  -= fill;
                if (resting.remaining == 0) queue.pop_front();
            }
            if (queue.empty()) bids_.erase(it);
        }
        // any remaining quantity is simply dropped — market orders never rest
    }
}

bool OrderBook::cancel_order(uint64_t order_id, Side side, double price) {
    return (side == Side::BUY) ? cancel_bid(order_id, price)
                               : cancel_ask(order_id, price);
}

bool OrderBook::cancel_bid(uint64_t order_id, double price) {
    auto it = bids_.find(price);
    if (it == bids_.end()) return false;

    auto& q = it->second;
    for (auto oit = q.begin(); oit != q.end(); ++oit) {
        if (oit->order_id == order_id) {
            q.erase(oit);
            if (q.empty()) bids_.erase(it);
            return true;
        }
    }
    return false;
}

bool OrderBook::cancel_ask(uint64_t order_id, double price) {
    auto it = asks_.find(price);
    if (it == asks_.end()) return false;

    auto& q = it->second;
    for (auto oit = q.begin(); oit != q.end(); ++oit) {
        if (oit->order_id == order_id) {
            q.erase(oit);
            if (q.empty()) asks_.erase(it);
            return true;
        }
    }
    return false;
}
// add these to the bottom of OrderBook.cpp if missing

double OrderBook::best_bid() const {
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::best_ask() const {
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

/*

---

## What we have so far

The matching engine is complete and correct. The full execution path for any order is:
```
add_order()
  ├── MARKET → match_market() → done (never rests)
  └── LIMIT  → match_limit() → if remaining > 0 → rest on book

*/