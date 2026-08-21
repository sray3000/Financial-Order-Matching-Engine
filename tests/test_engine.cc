#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

#include "Engine.h"

bool NearlyEqual(double a, double b, double epsilon = 1e-12) {
    return std::abs(a - b) < epsilon;
}

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
    std::cout << "PASSED ✅\n";
}

void TestPoolExhaustionReported() {
    std::cout << "Running Test: Pool Exhaustion Reported... ";
    Engine engine(1);

    assert(engine.bids.AddOrder({1, Side::Buy, 10000, 10, 1000}));
    assert(!engine.bids.AddOrder({2, Side::Buy, 9900, 10, 1001}));
    assert(engine.bids.orderMap.size() == 1);
    assert(engine.bids.orderMap.find(1) != engine.bids.orderMap.end());
    assert(engine.bids.orderMap.find(2) == engine.bids.orderMap.end());
    assert(engine.bids.orderBook.size() == 1);
    std::cout << "PASSED ✅\n";
}

void TestDuplicateOrderIdRejected() {
    std::cout << "Running Test: Duplicate Order ID rejected... ";

    Engine engine(10);

    Order first{
        1001,          // orderId
        Side::Buy,
        10000,         // price
        10,            // quantity
        1,             // timestamp
        OrderType::Limit
    };

    Order duplicate{
        1001,          // SAME orderId
        Side::Buy,
        10100,         // different price
        20,            // different quantity
        2,             // later timestamp
        OrderType::Limit
    };

    // First order should be accepted.
    assert(engine.bids.AddOrder(first));

    // Duplicate active order ID must be rejected.
    assert(!engine.bids.AddOrder(duplicate));

    // The original order must still be the active order.
    auto it = engine.bids.orderMap.find(1001);
    assert(it != engine.bids.orderMap.end());

    OrderNode* node = it->second;

    assert(node->order.orderId == 1001);
    assert(node->order.price == 10000);
    assert(node->order.quantity == 10);

    // Duplicate must not create another price level.
    assert(engine.bids.orderBook.size() == 1);
    assert(engine.bids.orderBook.find(10000) != engine.bids.orderBook.end());
    assert(engine.bids.orderBook.find(10100) == engine.bids.orderBook.end());

    std::cout << "PASSED ✅\n";
}

void TestCancelOnlyOrder() {
    std::cout << "Running Test: One order cancelled... ";

    Engine engine(10);

    Order order{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(order));

    assert(engine.bids.orderMap.size() == 1);
    assert(engine.bids.orderBook.size() == 1);

    engine.bids.CancelOrder(1001);

    // Order must disappear from the ID map.
    assert(engine.bids.orderMap.empty());

    // Empty price level must also disappear.
    assert(engine.bids.orderBook.empty());

    std::cout << "PASSED ✅\n";
}

void TestCancelHeadOrder() {
    std::cout << "Running Test: Order at head cancelled... ";

    Engine engine(10);

    Order first{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    Order second{
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    };

    Order third{
        1003,
        Side::Buy,
        10000,
        30,
        3,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(first));
    assert(engine.bids.AddOrder(second));
    assert(engine.bids.AddOrder(third));

    engine.bids.CancelOrder(1001);

    auto levelIt = engine.bids.orderBook.find(10000);
    assert(levelIt != engine.bids.orderBook.end());

    PriceLevel& level = levelIt->second;

    assert(level.head != nullptr);
    assert(level.tail != nullptr);

    // Second order must now be the oldest.
    assert(level.head->order.orderId == 1002);
    assert(level.tail->order.orderId == 1003);

    // First order must no longer exist.
    assert(engine.bids.orderMap.find(1001) ==
           engine.bids.orderMap.end());

    // Remaining orders must still exist.
    assert(engine.bids.orderMap.find(1002) !=
           engine.bids.orderMap.end());

    assert(engine.bids.orderMap.find(1003) !=
           engine.bids.orderMap.end());

    std::cout << "PASSED ✅\n";
}

void TestCancelMiddleOrder() {
    std::cout << "Running Test: Order in the middle cancelled... ";

    Engine engine(10);

    Order first{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    Order second{
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    };

    Order third{
        1003,
        Side::Buy,
        10000,
        30,
        3,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(first));
    assert(engine.bids.AddOrder(second));
    assert(engine.bids.AddOrder(third));

    engine.bids.CancelOrder(1002);

    auto levelIt = engine.bids.orderBook.find(10000);
    assert(levelIt != engine.bids.orderBook.end());

    PriceLevel& level = levelIt->second;

    assert(level.head->order.orderId == 1001);
    assert(level.tail->order.orderId == 1003);

    // Verify the links were correctly repaired.
    assert(level.head->next == level.tail);
    assert(level.tail->prev == level.head);

    assert(engine.bids.orderMap.find(1002) ==
           engine.bids.orderMap.end());

    std::cout << "PASSED ✅\n";
}

void TestCancelTailOrder() {
    std::cout << "Running Test: Order at tail cancelled... ";

    Engine engine(10);

    Order first{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    Order second{
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    };

    Order third{
        1003,
        Side::Buy,
        10000,
        30,
        3,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(first));
    assert(engine.bids.AddOrder(second));
    assert(engine.bids.AddOrder(third));

    engine.bids.CancelOrder(1003);

    auto levelIt = engine.bids.orderBook.find(10000);
    assert(levelIt != engine.bids.orderBook.end());

    PriceLevel& level = levelIt->second;

    assert(level.head->order.orderId == 1001);
    assert(level.tail->order.orderId == 1002);

    assert(level.tail->next == nullptr);

    assert(engine.bids.orderMap.find(1003) ==
           engine.bids.orderMap.end());

    std::cout << "PASSED ✅\n";
}

void TestCancelUnknownOrder() {
    std::cout << "Running Test: Unknown order cancelled... ";

    Engine engine(10);

    Order order{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(order));

    // Should do nothing and should not corrupt the book.
    engine.bids.CancelOrder(9999);

    assert(engine.bids.orderMap.size() == 1);
    assert(engine.bids.orderBook.size() == 1);

    auto it = engine.bids.orderMap.find(1001);
    assert(it != engine.bids.orderMap.end());

    assert(it->second->order.orderId == 1001);

    std::cout << "PASSED ✅\n";
}

void TestPoolReuseAfterCancellation() {
    std::cout << "Running Test: Pool reuse after cancellation... ";

    Engine engine(1);

    Order first{
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    };

    Order second{
        1002,
        Side::Buy,
        10100,
        20,
        2,
        OrderType::Limit
    };

    // Consume the only pool slot.
    assert(engine.bids.AddOrder(first));

    // Pool should now be exhausted.
    assert(!engine.bids.AddOrder(second));

    // Cancellation must return the node to the pool.
    engine.bids.CancelOrder(1001);

    // The pool slot should now be reusable.
    assert(engine.bids.AddOrder(second));

    assert(engine.bids.orderMap.size() == 1);

    auto it = engine.bids.orderMap.find(1002);
    assert(it != engine.bids.orderMap.end());

    assert(it->second->order.orderId == 1002);
    assert(it->second->order.price == 10100);

    std::cout << "PASSED ✅\n";
}

void TestReusableTradeBuffer() {
    std::cout << "Running Test: Reusable Trade Buffer... ";
    Engine engine(1000);
    std::vector<TradeEvent> trades;
    trades.reserve(2);

    engine.ProcessOrder({1, Side::Sell, 10000, 10, 1000}, trades);
    assert(trades.empty());
    engine.ProcessOrder({2, Side::Buy, 10000, 10, 1001}, trades);

    assert(trades.size() == 1);
    assert(trades[0].makerOrderId == 1);
    assert(trades[0].takerOrderId == 2);
    std::cout << "PASSED ✅\n";
}

void TestMarketOrderDoesNotRest() {
    std::cout << "Running Test: Market Order Matches and Does Not Rest... ";
    Engine engine(1000);

    engine.ProcessOrder({1, Side::Sell, 10000, 5, 1000});
    engine.ProcessOrder({2, Side::Sell, 10100, 5, 1001});

    // The price is ignored for a market order. It consumes all 10 available
    // units and discards its remaining 2 units rather than resting them.
    auto trades = engine.ProcessOrder({3, Side::Buy, 0, 12, 1002, OrderType::Market});

    assert(trades.size() == 2);
    assert(trades[0].price == 10000);
    assert(trades[0].quantity == 5);
    assert(trades[1].price == 10100);
    assert(trades[1].quantity == 5);
    assert(engine.asks.orderBook.empty());
    assert(engine.bids.orderBook.empty());
    assert(engine.bids.orderMap.empty());
    std::cout << "PASSED ✅\n";
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

void TestMarketMetricsEmptyBook() {
    std::cout << "Running Test: Market metrics of empty book... ";
    Engine engine(10);

    MarketMetrics metrics = engine.GetMarketMetrics();

    assert(!metrics.hasBid);
    assert(!metrics.hasAsk);

    assert(metrics.bestBid == 0);
    assert(metrics.bestAsk == 0);

    assert(metrics.bidQuantity == 0);
    assert(metrics.askQuantity == 0);

    assert(metrics.spread == 0);
    assert(metrics.midPrice == 0.0);
    assert(metrics.imbalance == 0.0);

    std::cout << "PASSED ✅\n";
}

void TestMarketMetricsOneSidedBook() {
    std::cout << "Running Test: Market metrics of one-sided book... ";
    Engine engine(10);

    Order bid{
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(bid));

    MarketMetrics metrics = engine.GetMarketMetrics();

    assert(metrics.hasBid);
    assert(!metrics.hasAsk);

    assert(metrics.bestBid == 10000);
    assert(metrics.bidQuantity == 30);

    assert(metrics.bestAsk == 0);
    assert(metrics.askQuantity == 0);

    // Spread and mid-price require both sides.
    assert(metrics.spread == 0);
    assert(metrics.midPrice == 0.0);

    // Entire top-of-book quantity is on the bid.
    assert(NearlyEqual(metrics.imbalance, 1.0));

    std::cout << "PASSED ✅\n";
}

void TestMarketMetricsBestBidAsk() {
    std::cout << "Running Test: Market metrics of best bid-ask... ";
    Engine engine(20);

    Order bid1{
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    };

    Order bid2{
        1002,
        Side::Buy,
        9900,
        100,
        2,
        OrderType::Limit
    };

    Order ask1{
        2001,
        Side::Sell,
        10100,
        40,
        3,
        OrderType::Limit
    };

    Order ask2{
        2002,
        Side::Sell,
        10200,
        100,
        4,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(bid1));
    assert(engine.bids.AddOrder(bid2));

    assert(engine.asks.AddOrder(ask1));
    assert(engine.asks.AddOrder(ask2));

    MarketMetrics metrics = engine.GetMarketMetrics();

    assert(metrics.hasBid);
    assert(metrics.hasAsk);

    // Best prices.
    assert(metrics.bestBid == 10000);
    assert(metrics.bestAsk == 10100);

    // Quantity at best prices only.
    assert(metrics.bidQuantity == 30);
    assert(metrics.askQuantity == 40);

    // 10100 - 10000 = 100 cents.
    assert(metrics.spread == 100);

    // (10000 + 10100) / 2 = 10050.
    assert(NearlyEqual(metrics.midPrice, 10050.0));

    // (30 - 40) / (30 + 40)
    assert(NearlyEqual(metrics.imbalance, static_cast<double>(-10) / 70.0));
    
    std::cout << "PASSED ✅\n";
}

void TestMarketMetricsAggregatesBestLevel() {
    std::cout << "Running Test: Market metrics of multiple orders aggregate... ";
    Engine engine(20);

    Order bid1{
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    };

    Order bid2{
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    };

    Order bid3{
        1003,
        Side::Buy,
        9900,
        100,
        3,
        OrderType::Limit
    };

    Order ask1{
        2001,
        Side::Sell,
        10100,
        50,
        4,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(bid1));
    assert(engine.bids.AddOrder(bid2));
    assert(engine.bids.AddOrder(bid3));
    assert(engine.asks.AddOrder(ask1));

    MarketMetrics metrics = engine.GetMarketMetrics();

    assert(metrics.bestBid == 10000);

    // 30 + 20 at the best bid.
    assert(metrics.bidQuantity == 50);

    assert(metrics.bestAsk == 10100);
    assert(metrics.askQuantity == 50);

    assert(metrics.spread == 100);
    assert(metrics.midPrice == 10050.0);

    // Equal quantities -> balanced book.
    assert(NearlyEqual(metrics.imbalance, 0.0));

    std::cout << "PASSED ✅\n";
}

void TestMarketMetricsAfterCancellation() {
    std::cout << "Running Test: Market metrics after cancellation... ";
    Engine engine(10);

    Order bid1{
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    };

    Order bid2{
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    };

    Order ask{
        2001,
        Side::Sell,
        10100,
        40,
        3,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(bid1));
    assert(engine.bids.AddOrder(bid2));
    assert(engine.asks.AddOrder(ask));

    MarketMetrics before = engine.GetMarketMetrics();

    assert(before.bidQuantity == 50);
    assert(before.askQuantity == 40);

    engine.CancelOrder(1001, Side::Buy);

    MarketMetrics after = engine.GetMarketMetrics();

    assert(after.bestBid == 10000);
    assert(after.bidQuantity == 20);

    assert(after.bestAsk == 10100);
    assert(after.askQuantity == 40);

    std::cout << "PASSED ✅\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    MATCHING ENGINE UNIT TEST SUITE     \n";
    std::cout << "========================================\n";
    
    TestRestingOrders();
    TestZeroQuantityOrderRejected();
    TestPoolExhaustionReported();
    TestDuplicateOrderIdRejected();
    TestCancelOnlyOrder();
    TestCancelHeadOrder();
    TestCancelMiddleOrder();
    TestCancelTailOrder();
    TestCancelUnknownOrder();
    TestPoolReuseAfterCancellation();
    TestReusableTradeBuffer();
    TestMarketOrderDoesNotRest();
    TestExactMatch();
    TestPartialFillMakerLarger();
    TestMultiLevelCrossing();
    TestPriceTimePriority();
    TestMarketMetricsEmptyBook();
    TestMarketMetricsOneSidedBook();
    TestMarketMetricsBestBidAsk();
    TestMarketMetricsAggregatesBestLevel();
    TestMarketMetricsAfterCancellation();

    std::cout << "\nALL UNIT TESTS PASSED SUCCESSFULLY! 🎉\n";
    std::cout << "========================================\n";
    return 0;
}
