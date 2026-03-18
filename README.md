# QuantumBook — Low Latency Order Matching System

A high-performance limit order book implemented in C++ with an event-driven matching engine, lock-free producer-consumer pipeline, and Python benchmarking harness.

---

## Architecture

```
Python Harness (benchmark.py)
        |
        | binary order stream
        v
  Ingestion Layer  [producer thread]
  - generates + validates orders
  - timestamps at submission (ns precision)
        |
        | Order structs
        v
  SPSC Ring Buffer  [lock-free, cache-line aligned]
  - power-of-2 capacity (65536 slots)
  - acquire/release memory ordering
  - heap-allocated slots (no stack overflow)
        |
        v
  Matching Engine  [consumer thread]
  - price-time priority
  - market + limit orders
  - partial fills
        |
       / \
      /   \
  Bid Side   Ask Side
  std::map   std::map
  (desc)     (asc)
  price -> deque<Order>
```

---

## Features

- **Price-time priority matching** — bids sorted descending, asks ascending via `std::map` with custom comparator. Best counterparty is always `.begin()` — no searching.
- **Market and limit orders** — limit orders attempt to match before resting; market orders sweep until filled and never rest on the book.
- **Partial fills** — orders track `remaining` quantity independently; price levels are cleaned up automatically when exhausted.
- **Lock-free SPSC ring buffer** — single producer, single consumer with `std::atomic` head/tail indices. `acquire/release` pairing provides minimum necessary synchronization with no mutex overhead.
- **Cache-line separation** — `head_` and `tail_` padded with `alignas(64)` to separate cache lines, eliminating false sharing between producer and consumer cores.
- **Heap-allocated slots** — ring buffer storage on the heap, avoiding stack overflow at large capacities.
- **Cancel support** — O(n) cancel within a price level via typed `cancel_bid` / `cancel_ask` helpers, no UB casts.

---

## Benchmark Results

Measured on Windows, MinGW-W64 GCC 15.2, `-O3`, single producer + single consumer thread.

| Batch      | Orders Processed | Trades Generated | Fill Rate | Wall Time |
|------------|-----------------|-----------------|-----------|-----------|
| 50K orders  | 50,000          | ~42,500         | ~85%      | 19 ms     |
| 100K orders | 100,000         | ~85,000         | ~85%      | 33 ms     |
| 300K orders | 300,000         | ~255,000        | ~85%      | 132 ms    |
| 500K orders | 500,000         | ~425,000        | ~85%      | 265 ms    |

**Derived throughput:** 500K orders in 265ms = **~1.9M orders/sec wall throughput**

**Latency (mean / p99):**

| Batch       | Mean    | P99     |
|-------------|---------|---------|
| 50K orders  | 7.5 ms  | 14.8 ms |
| 100K orders | 12.0 ms | 19.7 ms |
| 300K orders | 23.3 ms | 28.6 ms |
| 500K orders | 30.3 ms | 43.2 ms |

> Latency includes queue wait time — the timestamp is taken at submission, not dequeue. This is the honest end-to-end number.

**Wall time scales linearly** — slope of 0.551 ms per 1K orders confirms O(n) matching behavior with no degradation at scale.

**85% fill rate** is consistent across all batch sizes — a direct result of the tight ±$1 price spread in the order generator producing frequent crosses.

---

## File Structure

```
QuantumBook/
├── Order.h          # Order struct + Side/OrderType enums
├── OrderBook.h      # Limit order book interface + Trade struct
├── OrderBook.cpp    # Matching engine implementation
├── RingBuffer.h     # Lock-free SPSC ring buffer (templated)
├── Engine.h         # Threaded execution pipeline
├── main.cpp         # Benchmark runner + CSV export
├── benchmark.py     # Python dashboard (4-panel matplotlib output)
├── results.csv      # Benchmark output (auto-generated)
└── CMakeLists.txt   # CMake build config
```

---

## Build

### Windows (MinGW-W64 GCC 15+)

```bash
g++ -std=c++17 -O3 -o quantumbook main.cpp OrderBook.cpp
.\quantumbook.exe
```

### Linux / macOS

```bash
g++ -std=c++17 -O3 -pthread -o quantumbook main.cpp OrderBook.cpp
./quantumbook
```

### CMake (cross-platform)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./quantumbook
```

---

## Run the Python Dashboard

```bash
pip install pandas matplotlib numpy
python benchmark.py
```

Reads `results.csv` and produces a 4-panel PNG dashboard:
- Throughput by batch size
- Mean vs P99 matching latency
- Orders sent vs trades generated (fill rate %)
- Wall time vs batch size with linear fit

---

## Design Decisions

**Why `std::map` instead of `std::unordered_map`?**
The matching engine always needs the best price — the minimum ask or maximum bid. `std::map` keeps keys sorted, so best price is always `O(1)` via `.begin()`. An unordered map would require scanning all price levels on every match.

**Why SPSC over a general concurrent queue?**
With exactly one producer and one consumer, SPSC allows a fully lock-free design using only two atomic indices. No mutex, no condvar, no contention. A general MPMC queue adds unnecessary overhead for this architecture.

**Why `acquire/release` instead of `seq_cst`?**
`seq_cst` inserts full memory fences on x86, which are expensive. The SPSC pattern only requires the producer to `release` after writing a slot and the consumer to `acquire` before reading — this is the minimal synchronization needed for correctness.

**Why `alignas(64)` on `head_` and `tail_`?**
Without alignment, both atomics can share a single 64-byte cache line. Every producer write to `head_` would then invalidate the consumer's cached copy of `tail_`, and vice versa — false sharing that kills throughput on multi-core even though the two threads never touch the same variable.

**Why heap-allocated ring buffer slots?**
`RingBuffer<65536>` with 40-byte `Order` structs = 2.6MB. Windows default stack is 1MB. Stack allocation causes an immediate `0xC00000FD` stack overflow. Heap allocation via `new Order[Capacity]` moves this off the stack entirely.

**Why `cancel_bid` / `cancel_ask` instead of a templated helper?**
`bids_` uses `std::greater<double>` as comparator, `asks_` uses the default ascending comparator. They are different types — casting between them is undefined behavior. Two typed private methods each working on their own map directly is the correct, zero-UB approach.

---


