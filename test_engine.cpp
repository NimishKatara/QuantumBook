// test_engine.cpp
#include <windows.h>
#include <iostream>
#include <atomic>
#include <functional>

// ── paste minimal stubs so we don't drag in the whole project ─────────────────

#include "Order.h"
#include "RingBuffer.h"

std::atomic<bool> running(true);

DWORD WINAPI worker(LPVOID param) {
    std::cout << "[thread] started\n" << std::flush;

    RingBuffer<1024>* ring = static_cast<RingBuffer<1024>*>(param);

    int processed = 0;
    while (true) {
        MaybeOrder m = ring->pop();
        if (!m.valid) {
            if (!running.load()) break;
            Sleep(0);
            continue;
        }
        processed++;
    }
    std::cout << "[thread] processed=" << processed << "\n" << std::flush;
    return 0;
}

int main() {
    std::cout << "[1] constructing ring\n"   << std::flush;
    RingBuffer<1024> ring;

    std::cout << "[2] launching thread\n"    << std::flush;
    HANDLE h = CreateThread(nullptr, 0, worker, &ring, 0, nullptr);
    if (!h) { std::cout << "FAILED\n"; return 1; }

    std::cout << "[3] pushing 500 orders\n"  << std::flush;
    for (int i = 1; i <= 500; i++) {
        Order o(i, 0, Side::BUY, OrderType::LIMIT, 100.0, 10);
        while (!ring.push(o)) Sleep(0);
    }

    std::cout << "[4] stopping\n"            << std::flush;
    running.store(false);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);

    std::cout << "[5] done\n"                << std::flush;
    return 0;
}