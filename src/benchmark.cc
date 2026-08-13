#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include "Engine.cc"

int main() {
    std::cout << "Initializing matching engine for benchmark (" << NUM_ORDERS << " orders)...\n";

    constexpr size_t marketOrderCycle = 20;
    
    // Pre-generate orders to eliminate random generation overhead during the measurement loop
    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    uint64_t basePrice = 10000; // Representing $100.00 in fixed-point cents
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        Order o;
        o.orderId = i + 1;
        // Alternate evenly between buy and sell orders. Two orders per
        // twenty-order cycle are market orders (one buy and one sell), for a
        // balanced 10% market-order workload.
        o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        const size_t cyclePosition = i % marketOrderCycle;
        o.type = (cyclePosition == 0 || cyclePosition == 11)
            ? OrderType::Market
            : OrderType::Limit;
        o.price = (o.type == OrderType::Market) ? 0 : basePrice + (i % 20) - 10;
        o.quantity = 10;
        o.timestamp = i;
        orders.push_back(o);
    }

    // Vector to store individual execution latencies in nanoseconds
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_ORDERS);

    std::cout << "Running warm-up phase...\n";
    // Warm up a separate engine so duplicate IDs and warm-up book state cannot
    // affect the measured workload.
    Engine warmupEngine(10000);
    std::vector<TradeEvent> warmupTrades;
    warmupTrades.reserve(NUM_ORDERS);
    for (size_t i = 0; i < 1000; ++i) {
        warmupEngine.ProcessOrder(orders[i], warmupTrades);
    }

    // Start measurement from a clean book with enough pool capacity.
    Engine engine(NUM_ORDERS + 10000);

    // Reused output storage keeps result-vector allocation out of every timed call.
    std::vector<TradeEvent> trades;
    trades.reserve(NUM_ORDERS);

    std::cout << "Starting high-precision measurement loop...\n";
    
    auto total_start = std::chrono::high_resolution_clock::now();

    // Core timing loop
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Execute the matching engine path
        engine.ProcessOrder(orders[i], trades);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        uint64_t durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(durationNs);
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

    // Sort latencies in ascending order to accurately calculate percentiles
    std::sort(latencies.begin(), latencies.end());

    // Compute metrics
    uint64_t total_ns = std::accumulate(latencies.begin(), latencies.end(), uint64_t(0));
    double avg_latency = static_cast<double>(total_ns) / NUM_ORDERS;
    uint64_t p50 = latencies[NUM_ORDERS * 0.50];
    uint64_t p99 = latencies[static_cast<size_t>(NUM_ORDERS * 0.99)];
    uint64_t p999 = latencies[static_cast<size_t>(NUM_ORDERS * 0.999)];
    double ops = (static_cast<double>(NUM_ORDERS) / total_duration_ms) * 1000.0;

    // Display formatted results report
    std::cout << "\n========================================\n";
    std::cout << "       MATCHING ENGINE BENCHMARK        \n";
    std::cout << "========================================\n";
    std::cout << "Total Orders Processed : " << NUM_ORDERS << "\n";
    std::cout << "Total Time Elapsed     : " << total_duration_ms << " ms\n";
    std::cout << "Throughput (OPS)       : " << static_cast<size_t>(ops) << " orders/sec\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Latency Statistics (Nanoseconds):\n";
    std::cout << "  Average              : " << avg_latency << " ns\n";
    std::cout << "  Median (p50)         : " << p50 << " ns\n";
    std::cout << "  99th Percentile (p99): " << p99 << " ns\n";
    std::cout << "  99.9th Percentile    : " << p999 << " ns\n";
    std::cout << "========================================\n";

    return 0;
}
