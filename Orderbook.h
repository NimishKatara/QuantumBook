// OrderBook.h
#pragma once
#include "Order.h"
#include <map>
#include <deque>
#include <functional>
#include <vector>

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    double   price;
    uint32_t quantity;
};

using TradeCallback = std::function<void(const Trade&)>;

class OrderBook {
public:
    // no-op default — lets Engine rebind callback after construction
    OrderBook() : on_trade_([](const Trade&){}) {}

    explicit OrderBook(TradeCallback cb) : on_trade_(std::move(cb)) {}

    OrderBook& operator=(OrderBook&& other) = default;

    void add_order(Order order);
    bool cancel_order(uint64_t order_id, Side side, double price);

    double best_bid() const;
    double best_ask() const;

private:
    std::map<double, std::deque<Order>, std::greater<double>> bids_;
    std::map<double, std::deque<Order>>                       asks_;

    TradeCallback on_trade_;

    void match_limit(Order& incoming);
    void match_market(Order& incoming);

    bool cancel_bid(uint64_t order_id, double price);
    bool cancel_ask(uint64_t order_id, double price);
};