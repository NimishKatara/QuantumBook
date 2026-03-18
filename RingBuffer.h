// RingBuffer.h
#pragma once
#include "Order.h"
#include <atomic>
#include <array>
#include <memory>

struct MaybeOrder {
    Order order;
    bool  valid;
};

template<std::size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    RingBuffer() : head_(0), tail_(0),
                   slots_(new Order[Capacity])   
    {}

    ~RingBuffer() {
        delete[] slots_;
    }

    RingBuffer(const RingBuffer&)            = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    bool push(const Order& order) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & mask_;

        if (next == tail_.load(std::memory_order_acquire))
            return false;

        slots_[head] = order;
        head_.store(next, std::memory_order_release);
        return true;
    }

    MaybeOrder pop() {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire))
            return { Order(), false };

        Order order = slots_[tail];
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return { order, true };
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t mask_ = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;

    Order* slots_;   
};