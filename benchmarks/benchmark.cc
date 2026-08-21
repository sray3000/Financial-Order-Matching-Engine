#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "Engine.h"

enum class Workload {
    Balanced,
    RestingHeavy,
    Aggressive
};

struct BenchmarkConfig {
    const char* name;
    Workload workload;
    size_t orderCount;
};

struct ThroughputResult {
    double elapsedSeconds;
    double ordersPerSecond;

    size_t trades;
    double tradesPerOrder;

};

struct LatencyResult {
    uint64_t p50;
    uint64_t p99;
    uint64_t p999;
};

constexpr size_t BENCHMARK_ORDERS = 1'000'000;

const BenchmarkConfig workloads[] = {
    {"Balanced",      Workload::Balanced,     BENCHMARK_ORDERS},
    {"Resting-heavy", Workload::RestingHeavy, BENCHMARK_ORDERS},
    {"Aggressive",    Workload::Aggressive,   BENCHMARK_ORDERS}
};


// ------------------------------------------------------------
// Order generation
// ------------------------------------------------------------

Order GenerateOrder(size_t index, Workload workload) {
    Order order{};

    order.orderId = index + 1;
    order.timestamp = index;
    order.quantity = 10;

    switch (workload) {

    case Workload::Balanced:
        order.side =
            (index % 2 == 0)
                ? Side::Buy
                : Side::Sell;

        order.type =
            (index % 10 == 0)
                ? OrderType::Market
                : OrderType::Limit;

        if (order.type == OrderType::Market) {
            order.price = 0;
        } else {
            order.price =
                10000 +
                static_cast<uint64_t>(index % 21) * 10;
        }

        break;


    case Workload::RestingHeavy:
        order.side =
            (index % 2 == 0)
                ? Side::Buy
                : Side::Sell;

        order.type = OrderType::Limit;

        // Keep bids below the ask region and asks above
        // the bid region so most orders rest.
        if (order.side == Side::Buy) {
            order.price =
                9900 -
                static_cast<uint64_t>(index % 10) * 10;
        } else {
            order.price =
                10100 +
                static_cast<uint64_t>(index % 10) * 10;
        }

        break;


    case Workload::Aggressive:
        order.side =
            (index % 2 == 0)
                ? Side::Buy
                : Side::Sell;

        // Alternate market buys and aggressive limit sells.
        if (order.side == Side::Buy) {
            order.type = OrderType::Market;
            order.price = 0;
        } else {
            order.type = OrderType::Limit;
            order.price = 9900;
        }

        break;
    }

    return order;
}


std::vector<Order> GenerateWorkload(
    size_t count,
    Workload workload
) {
    std::vector<Order> orders;
    orders.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        orders.push_back(
            GenerateOrder(i, workload)
        );
    }

    return orders;
}


// ------------------------------------------------------------
// Warm-up
// ------------------------------------------------------------

void Warmup(const std::vector<Order>& orders) {
    Engine engine(MAX_ORDERS);

    std::vector<TradeEvent> trades;
    trades.reserve(orders.size());

    const size_t warmupCount =
        std::min<size_t>(orders.size(), 10'000);

    for (size_t i = 0; i < warmupCount; ++i) {
        trades.clear();

        engine.ProcessOrder(
            orders[i],
            trades
        );
    }
}


// ------------------------------------------------------------
// Throughput benchmark
// ------------------------------------------------------------

ThroughputResult RunThroughputBenchmark(
    const std::vector<Order>& orders
) {
    // Fresh engine for every workload.
    Engine engine(MAX_ORDERS);

    // Reused output buffer keeps allocation outside
    // the matching path.
    std::vector<TradeEvent> trades;
    trades.reserve(orders.size());

    size_t tradeCount = 0;

    const auto start =
        std::chrono::steady_clock::now();

    for (const Order& order : orders) {
        trades.clear();

        engine.ProcessOrder(
            order,
            trades
        );

        tradeCount += trades.size();
    }

    const auto end =
        std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed =
        end - start;

    const double seconds =
        elapsed.count();

    const double ordersPerSecond =
        static_cast<double>(orders.size()) /
        seconds;

    const double tradesPerOrder =
        static_cast<double>(tradeCount) /
        static_cast<double>(orders.size());

    return {
        seconds,
        ordersPerSecond,
        tradeCount,
        tradesPerOrder
    };
}


// ------------------------------------------------------------
// Latency benchmark
// ------------------------------------------------------------

LatencyResult RunLatencyBenchmark(
    const std::vector<Order>& orders
) {
    // Fresh engine so latency measurement starts from
    // an uncontaminated book.
    Engine engine(MAX_ORDERS);

    std::vector<TradeEvent> trades;
    trades.reserve(orders.size());

    std::vector<uint64_t> latencies;
    latencies.reserve(orders.size());

    for (const Order& order : orders) {
        trades.clear();

        const auto start =
            std::chrono::steady_clock::now();

        engine.ProcessOrder(
            order,
            trades
        );

        const auto end =
            std::chrono::steady_clock::now();

        const uint64_t latency =
            static_cast<uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::nanoseconds
                >(end - start).count()
            );

        latencies.push_back(latency);
    }

    std::sort(
        latencies.begin(),
        latencies.end()
    );

    auto percentile =
        [&](double p) -> uint64_t {
            const size_t index =
                static_cast<size_t>(
                    p *
                    static_cast<double>(
                        latencies.size() - 1
                    )
                );

            return latencies[index];
        };

    return {
        percentile(0.50),
        percentile(0.99),
        percentile(0.999)
    };
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {

    std::cout
        << "========================================\n"
        << "     MATCHING ENGINE BENCHMARK          \n"
        << "========================================\n"
        << "Orders per workload : "
        << BENCHMARK_ORDERS
        << "\n\n";


    for (const BenchmarkConfig& config : workloads) {

        std::cout
            << "----------------------------------------\n"
            << "Workload: "
            << config.name
            << "\n"
            << "----------------------------------------\n";

        std::cout
            << "Generating "
            << config.orderCount
            << " orders...\n";

        std::vector<Order> orders =
            GenerateWorkload(
                config.orderCount,
                config.workload
            );

        std::cout
            << "Running warm-up...\n";

        Warmup(orders);

        // ----------------------------------------------------
        // Throughput
        // ----------------------------------------------------

        std::cout
            << "Running throughput benchmark...\n";

        ThroughputResult throughput =
            RunThroughputBenchmark(orders);

        // ----------------------------------------------------
        // Latency
        // ----------------------------------------------------

        std::cout
            << "Running latency benchmark...\n";

        LatencyResult latency =
            RunLatencyBenchmark(orders);

        // ----------------------------------------------------
        // Results
        // ----------------------------------------------------

        std::cout
            << "\n";

        std::cout
            << std::fixed
            << std::setprecision(2);

        std::cout
            << "Throughput\n"
            << "  Time            : "
            << throughput.elapsedSeconds
            << " s\n"
            << "  Orders/sec      : "
            << throughput.ordersPerSecond
            << "\n"
            << "  Trades          : "
            << throughput.trades
            << "\n"
            << "  Trades/order    : "
            << throughput.tradesPerOrder
            << "\n";

        std::cout
            << "\nLatency (nanoseconds)\n"
            << "  p50             : "
            << latency.p50
            << "\n"
            << "  p99             : "
            << latency.p99
            << "\n"
            << "  p99.9           : "
            << latency.p999
            << "\n\n";
    }

    std::cout
        << "========================================\n"
        << "Benchmark complete.\n"
        << "========================================\n";

    return 0;
}