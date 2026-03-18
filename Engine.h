// Engine.h
#pragma once
#include "OrderBook.h"
#include "RingBuffer.h"
#include <atomic>
#include <chrono>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <windows.h>

struct EngineStats {
    uint64_t orders_processed = 0;
    uint64_t trades_generated = 0;
    double   mean_latency_ns  = 0.0;
    double   p99_latency_ns   = 0.0;
    double   throughput_ops   = 0.0;
    double   elapsed_ms_ = 0.0;
};

constexpr std::size_t RING_CAPACITY = 1 << 16;

class Engine {
public:
    Engine() : book_(nullptr), running_(false), thread_handle_(nullptr) {
        // construct on heap AFTER 'this' exists — no capture timing issue
        book_ = new OrderBook([this](const Trade& t){ on_trade(t); });
    }

    ~Engine() {
        stop();
        delete book_;
    }

    void start() {
        running_.store(true, std::memory_order_release);
        thread_handle_ = CreateThread(
            nullptr, 0, &Engine::thread_func, this, 0, nullptr
        );
        if (thread_handle_ == nullptr)
            std::cerr << "CreateThread failed: " << GetLastError() << "\n";
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (thread_handle_ != nullptr) {
            WaitForSingleObject(thread_handle_, INFINITE);
            CloseHandle(thread_handle_);
            thread_handle_ = nullptr;
        }
    }

    bool submit(Order order) {
        order.timestamp = now_ns();
        while (!ring_.push(order))
            Sleep(0);
        return true;
    }

    const EngineStats& stats() const { return stats_; }

    void set_elapsed_ms(double ms) {
        stats_.elapsed_ms_ = ms;
    }

private:
    OrderBook*                book_;
    RingBuffer<RING_CAPACITY> ring_;
    std::atomic<bool>         running_;
    EngineStats               stats_;
    std::vector<double>       latencies_;
    HANDLE                    thread_handle_;

    static DWORD WINAPI thread_func(LPVOID param) {
        static_cast<Engine*>(param)->consume_loop();
        return 0;
    }

    void consume_loop() {
        while (true) {
            MaybeOrder maybe = ring_.pop();

            if (!maybe.valid) {
                if (!running_.load(std::memory_order_acquire))
                    break;
                Sleep(0);
                continue;
            }

            Order order = maybe.order;
            book_->add_order(order);

            uint64_t latency = now_ns() - order.timestamp;
            latencies_.push_back(static_cast<double>(latency));
            stats_.orders_processed++;
        }
        compute_stats();
    }

    void on_trade(const Trade& t) {
        stats_.trades_generated++;
        (void)t;
    }

    void compute_stats() {
        if (latencies_.empty()) return;

    double sum = 0.0;
    for (double v : latencies_) sum += v;
    stats_.mean_latency_ns = sum / latencies_.size();

    std::sort(latencies_.begin(), latencies_.end());
    std::size_t p99_idx = static_cast<std::size_t>(latencies_.size() * 0.99);
    stats_.p99_latency_ns = latencies_[p99_idx];

    // throughput from wall time — honest number
    if (stats_.elapsed_ms_ > 0)
        stats_.throughput_ops = (static_cast<double>(stats_.orders_processed)
                                / stats_.elapsed_ms_) * 1000.0;
}

    static uint64_t now_ns() {
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
    }
};