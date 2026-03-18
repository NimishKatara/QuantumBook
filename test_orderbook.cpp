// test_orderbook.cpp
#include "OrderBook.h"
#include <iostream>

int main() {
    std::cout << "[1] constructing orderbook\n" << std::flush;

    OrderBook book([](const Trade& t){
        std::cout << "  trade: qty=" << t.quantity
                  << " px="  << t.price << "\n" << std::flush;
    });

    std::cout << "[2] adding resting bid\n" << std::flush;
    book.add_order(Order(1, 0, Side::BUY, OrderType::LIMIT, 100.0, 10));

    std::cout << "[3] adding resting ask\n" << std::flush;
    book.add_order(Order(2, 0, Side::SELL, OrderType::LIMIT, 101.0, 10));

    std::cout << "[4] best bid=" << book.best_bid()
              << " ask="         << book.best_ask() << "\n" << std::flush;

    std::cout << "[5] sending crossing sell\n" << std::flush;
    book.add_order(Order(3, 0, Side::SELL, OrderType::LIMIT, 99.0, 5));

    std::cout << "[6] sending market buy\n" << std::flush;
    book.add_order(Order(4, 0, Side::BUY, OrderType::MARKET, 0.0, 10));

    std::cout << "[7] done\n" << std::flush;
    return 0;
}