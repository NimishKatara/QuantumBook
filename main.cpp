// main.cpp
#include "Engine.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <random>
#include <vector>
#include <chrono>
#include <string>

// ── order generation ──────────────────────────────────────────────────────────

struct OrderGen {
    std::mt19937_64                  rng;
    std::uniform_real_distribution<> price_dist;
    std::uniform_int_distribution<>  qty_dist;
    std::uniform_int_distribution<>  side_dist;
    std::uniform_int_distribution<>  type_dist;
    uint64_t                         next_id = 1;

    OrderGen()
        : rng(std::random_device{}()),
          price_dist(99.0, 101.0),
          qty_dist(1, 100),
          side_dist(0, 1),
          type_dist(0, 9)
    {}

    Order next() {
        Side      s = side_dist(rng)   ? Side::BUY : Side::SELL;
        OrderType t = (type_dist(rng) == 0) ? OrderType::MARKET
                                             : OrderType::LIMIT;
        double    p = (t == OrderType::MARKET) ? 0.0 : price_dist(rng);
        uint32_t  q = qty_dist(rng);
        return Order(next_id++, 0, s, t, p, q);
    }
};

// ── benchmark result ──────────────────────────────────────────────────────────

struct BenchResult {
    uint64_t orders_sent;
    uint64_t orders_processed;
    uint64_t trades_generated;
    double   mean_latency_ns;
    double   p99_latency_ns;
    double   throughput_ops;
    double   elapsed_ms;
};

// ── benchmark runner ──────────────────────────────────────────────────────────

BenchResult run_benchmark(uint64_t num_orders) {
    Engine   engine;
    OrderGen gen;

    engine.start();
    auto wall_start = std::chrono::steady_clock::now();

    for (uint64_t i = 0; i < num_orders; i++)
        engine.submit(gen.next());

    engine.stop();
    auto wall_end  = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(
                         wall_end - wall_start).count();

    engine.set_elapsed_ms(elapsed);   // pass wall time before reading stats

    const EngineStats& s = engine.stats();
    return { num_orders, s.orders_processed, s.trades_generated,
             s.mean_latency_ns, s.p99_latency_ns,
             s.throughput_ops, elapsed };
}

// ── pretty printer ────────────────────────────────────────────────────────────

void print_result(const std::string& label, const BenchResult& r) {
    std::cout << "\n+-- " << label << "\n"
              << "|  orders sent       : " << r.orders_sent        << "\n"
              << "|  orders processed  : " << r.orders_processed   << "\n"
              << "|  trades generated  : " << r.trades_generated   << "\n"
              << std::fixed << std::setprecision(2)
              << "|  mean latency      : " << r.mean_latency_ns    << " ns\n"
              << "|  p99  latency      : " << r.p99_latency_ns     << " ns\n"
              << "|  throughput        : " << r.throughput_ops / 1000.0
                                           << " K orders/sec\n"
              << "|  wall time         : " << r.elapsed_ms         << " ms\n"
              << "+---------------------------------------------\n";
}

// ── csv export ────────────────────────────────────────────────────────────────

void write_csv(const std::vector<std::pair<std::string, BenchResult>>& runs) {
    std::ofstream f("results.csv");
    f << "label,orders_sent,orders_processed,trades,mean_ns,p99_ns,"
         "throughput_kops,elapsed_ms\n";

    for (std::size_t i = 0; i < runs.size(); i++) {
        const std::string& label = runs[i].first;
        const BenchResult& r     = runs[i].second;
        f << label                     << ","
          << r.orders_sent             << ","
          << r.orders_processed        << ","
          << r.trades_generated        << ","
          << r.mean_latency_ns         << ","
          << r.p99_latency_ns          << ","
          << r.throughput_ops / 1000.0 << ","
          << r.elapsed_ms              << "\n";
    }
    std::cout << "\nresults written to results.csv\n";
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "QuantumBook - Low Latency Order Matching Benchmark\n"
              << "====================================================\n"
              << std::flush;

    std::vector<std::pair<std::string, BenchResult>> runs;

    for (uint64_t n : { 50000ULL, 100000ULL, 300000ULL, 500000ULL }) {
        std::string label = std::to_string(n / 1000) + "K orders";
        std::cout << "\nrunning: " << label << " ..." << std::flush;

        BenchResult r = run_benchmark(n);
        print_result(label, r);
        runs.emplace_back(label, r);
    }

    write_csv(runs);
    return 0;
}