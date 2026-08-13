#include <iostream>
#include <cassert>
#include <vector>
#include "Engine.cc"

void TestRestingOrders() {
    std::cout << "Running Test: Resting Orders (No Match)... ";
    Engine engine(1000);

    // 1. Add a Buy order to an empty book
    auto trades1 = engine.ProcessOrder({1, Side::Buy, 10000, 10, 1000});
    assert(trades1.empty());

    // 2. Add a Sell order at a higher price (wide spread, no cross)
    auto trades2 = engine.ProcessOrder({2, Side::Sell, 10500, 10, 1001});
    assert(trades2.empty());
    std::cout << "PASSED ✅\n";
}

void TestZeroQuantityOrderRejected() {
    std::cout << "Running Test: Zero-Quantity Order Rejected... ";
    Engine engine(1000);

    auto buyTrades = engine.ProcessOrder({1, Side::Buy, 10000, 0, 1000});
    auto sellTrades = engine.ProcessOrder({2, Side::Sell, 10000, 0, 1001});

    assert(buyTrades.empty());
    assert(sellTrades.empty());
    assert(engine.bids.orderBook.empty());
    assert(engine.asks.orderBook.empty());
    assert(engine.bids.orderMap.empty());
    assert(engine.asks.orderMap.empty());
    std::cout << "PASSED\n";
}

void TestExactMatch() {
    std::cout << "Running Test: Exact Match (Full Fill)... ";
    Engine engine(1000);

    // Place resting Sell order
    engine.ProcessOrder({1, Side::Sell, 10000, 10, 1000});

    // Incoming Buy order matches exactly
    auto trades = engine.ProcessOrder({2, Side::Buy, 10000, 10, 1001});

    assert(trades.size() == 1);
    assert(trades[0].makerOrderId == 1);
    assert(trades[0].takerOrderId == 2);
    assert(trades[0].price == 10000);
    assert(trades[0].quantity == 10);
    std::cout << "PASSED ✅\n";
}

void TestPartialFillMakerLarger() {
    std::cout << "Running Test: Partial Fill (Maker Larger than Taker)... ";
    Engine engine(1000);

    // Place resting large Sell order
    engine.ProcessOrder({1, Side::Sell, 10000, 20, 1000});

    // Incoming smaller Buy order
    auto trades = engine.ProcessOrder({2, Side::Buy, 10000, 5, 1001});

    assert(trades.size() == 1);
    assert(trades[0].quantity == 5);
    assert(trades[0].makerOrderId == 1);
    std::cout << "PASSED ✅\n";
}

void TestMultiLevelCrossing() {
    std::cout << "Running Test: Multi-Level Crossing (Taker Consumes Multiple Asks)... ";
    Engine engine(1000);

    // Seed two levels of sell liquidity
    engine.ProcessOrder({1, Side::Sell, 10000, 5, 1000});  // Level 1: 5 units @ $100.00
    engine.ProcessOrder({2, Side::Sell, 10100, 10, 1001}); // Level 2: 10 units @ $101.00

    // Taker Buy order of 12 units at $101.00 crosses both levels
    auto trades = engine.ProcessOrder({3, Side::Buy, 10100, 12, 1002});

    assert(trades.size() == 2);
    // First execution: fully consumes maker 1 at $100.00
    assert(trades[0].makerOrderId == 1);
    assert(trades[0].price == 10000);
    assert(trades[0].quantity == 5);
    // Second execution: partially consumes maker 2 at $101.00
    assert(trades[1].makerOrderId == 2);
    assert(trades[1].price == 10100);
    assert(trades[1].quantity == 7);
    std::cout << "PASSED ✅\n";
}

void TestPriceTimePriority() {
    std::cout << "Running Test: Price-Time Priority (FIFO at Same Price)... ";
    Engine engine(1000);

    // Two sell orders at the exact same price level
    engine.ProcessOrder({1, Side::Sell, 10000, 10, 1000}); // Arrived earlier
    engine.ProcessOrder({2, Side::Sell, 10000, 10, 1005}); // Arrived later

    // Incoming Buy order matching 10 units
    auto trades = engine.ProcessOrder({3, Side::Buy, 10000, 10, 1010});

    assert(trades.size() == 1);
    // Must match Order 1 first based on time priority
    assert(trades[0].makerOrderId == 1);
    assert(trades[0].quantity == 10);
    std::cout << "PASSED ✅\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    MATCHING ENGINE UNIT TEST SUITE     \n";
    std::cout << "========================================\n";
    
    TestRestingOrders();
    TestZeroQuantityOrderRejected();
    TestExactMatch();
    TestPartialFillMakerLarger();
    TestMultiLevelCrossing();
    TestPriceTimePriority();

    std::cout << "\nALL UNIT TESTS PASSED SUCCESSFULLY! 🎉\n";
    std::cout << "========================================\n";
    return 0;
}
