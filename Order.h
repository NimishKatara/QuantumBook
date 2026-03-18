// Order.h
#pragma once
#include <cstdint>

enum class Side      : uint8_t { BUY, SELL };
enum class OrderType : uint8_t { LIMIT, MARKET };

struct Order {
    uint64_t  order_id;
    uint64_t  timestamp;
    double    price;
    uint32_t  quantity;
    uint32_t  remaining;
    Side      side;
    OrderType type;

    // default constructor — required by std::array inside RingBuffer
    Order()
        : order_id(0), timestamp(0), price(0.0),
          quantity(0), remaining(0),
          side(Side::BUY), type(OrderType::LIMIT)
    {}

    Order(uint64_t id, uint64_t ts, Side s, OrderType t, double p, uint32_t qty)
        : order_id(id), timestamp(ts), price(p),
          quantity(qty), remaining(qty), side(s), type(t)
    {}
};