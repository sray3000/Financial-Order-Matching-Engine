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

void TestEmptyOrderBookSnapshot() {
    std::cout << "Running Test: Empty order book snapshot... ";
    Engine engine(20);

    OrderBookSnapshot snapshot = engine.GetSnapshot(5);

    assert(snapshot.bids.empty());
    assert(snapshot.asks.empty());

    std::cout << "PASSED ✅\n";
}

void TestOrderBookSnapshotDepth() {
    std::cout << "Running Test: Order book snapshot depth... ";
    Engine engine(20);

    // Bids
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
        20,
        2,
        OrderType::Limit
    };

    Order bid3{
        1003,
        Side::Buy,
        9800,
        40,
        3,
        OrderType::Limit
    };

    // Asks
    Order ask1{
        2001,
        Side::Sell,
        10100,
        50,
        4,
        OrderType::Limit
    };

    Order ask2{
        2002,
        Side::Sell,
        10200,
        60,
        5,
        OrderType::Limit
    };

    Order ask3{
        2003,
        Side::Sell,
        10300,
        70,
        6,
        OrderType::Limit
    };

    assert(engine.bids.AddOrder(bid1));
    assert(engine.bids.AddOrder(bid2));
    assert(engine.bids.AddOrder(bid3));

    assert(engine.asks.AddOrder(ask1));
    assert(engine.asks.AddOrder(ask2));
    assert(engine.asks.AddOrder(ask3));

    OrderBookSnapshot snapshot = engine.GetSnapshot(3);

    assert(snapshot.bids.size() == 3);
    assert(snapshot.asks.size() == 3);

    // Bids: highest price first.
    assert(snapshot.bids[0].price == 10000);
    assert(snapshot.bids[0].quantity == 30);

    assert(snapshot.bids[1].price == 9900);
    assert(snapshot.bids[1].quantity == 20);

    assert(snapshot.bids[2].price == 9800);
    assert(snapshot.bids[2].quantity == 40);

    // Asks: lowest price first.
    assert(snapshot.asks[0].price == 10100);
    assert(snapshot.asks[0].quantity == 50);

    assert(snapshot.asks[1].price == 10200);
    assert(snapshot.asks[1].quantity == 60);

    assert(snapshot.asks[2].price == 10300);
    assert(snapshot.asks[2].quantity == 70);

    std::cout << "PASSED ✅\n";
}

void TestOrderBookSnapshotLimitedDepth() {
    std::cout << "Running Test: Order book snapshot limited depth... ";
    Engine engine(20);

    assert(engine.bids.AddOrder({
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1002,
        Side::Buy,
        9900,
        20,
        2,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1003,
        Side::Buy,
        9800,
        30,
        3,
        OrderType::Limit
    }));

    assert(engine.asks.AddOrder({
        2001,
        Side::Sell,
        10100,
        40,
        4,
        OrderType::Limit
    }));

    assert(engine.asks.AddOrder({
        2002,
        Side::Sell,
        10200,
        50,
        5,
        OrderType::Limit
    }));

    assert(engine.asks.AddOrder({
        2003,
        Side::Sell,
        10300,
        60,
        6,
        OrderType::Limit
    }));

    OrderBookSnapshot snapshot = engine.GetSnapshot(2);

    assert(snapshot.bids.size() == 2);
    assert(snapshot.asks.size() == 2);

    assert(snapshot.bids[0].price == 10000);
    assert(snapshot.bids[1].price == 9900);

    assert(snapshot.asks[0].price == 10100);
    assert(snapshot.asks[1].price == 10200);

    std::cout << "PASSED ✅\n";
}

void TestOrderBookSnapshotAggregatesPriceLevel() {
    std::cout << "Running Test: Order book snapshot aggregate price level... ";
    Engine engine(20);

    assert(engine.bids.AddOrder({
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1003,
        Side::Buy,
        9900,
        100,
        3,
        OrderType::Limit
    }));

    assert(engine.asks.AddOrder({
        2001,
        Side::Sell,
        10100,
        40,
        4,
        OrderType::Limit
    }));

    OrderBookSnapshot snapshot = engine.GetSnapshot(2);

    assert(snapshot.bids.size() == 2);

    assert(snapshot.bids[0].price == 10000);
    assert(snapshot.bids[0].quantity == 50);

    assert(snapshot.bids[1].price == 9900);
    assert(snapshot.bids[1].quantity == 100);

    assert(snapshot.asks.size() == 1);
    assert(snapshot.asks[0].price == 10100);
    assert(snapshot.asks[0].quantity == 40);

    std::cout << "PASSED ✅\n";
}

void TestOrderBookSnapshotZeroDepth() {
    std::cout << "Running Test: Order book snapshot zero depth... ";
    Engine engine(10);

    assert(engine.bids.AddOrder({
        1001,
        Side::Buy,
        10000,
        10,
        1,
        OrderType::Limit
    }));

    assert(engine.asks.AddOrder({
        2001,
        Side::Sell,
        10100,
        10,
        2,
        OrderType::Limit
    }));

    OrderBookSnapshot snapshot = engine.GetSnapshot(0);

    assert(snapshot.bids.empty());
    assert(snapshot.asks.empty());

    std::cout << "PASSED ✅\n";
}

void TestOrderBookSnapshotAfterCancellation() {
    std::cout << "Running Test: Order book snapshot after cancellation... ";
    Engine engine(20);

    assert(engine.bids.AddOrder({
        1001,
        Side::Buy,
        10000,
        30,
        1,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1002,
        Side::Buy,
        10000,
        20,
        2,
        OrderType::Limit
    }));

    assert(engine.bids.AddOrder({
        1003,
        Side::Buy,
        9900,
        100,
        3,
        OrderType::Limit
    }));

    engine.bids.CancelOrder(1001);

    OrderBookSnapshot snapshot = engine.GetSnapshot(2);

    assert(snapshot.bids.size() == 2);

    assert(snapshot.bids[0].price == 10000);
    assert(snapshot.bids[0].quantity == 20);

    assert(snapshot.bids[1].price == 9900);
    assert(snapshot.bids[1].quantity == 100);

    std::cout << "PASSED ✅\n";
}

void TestTradeEventMakerTakerSemantics() {
    std::cout << "Running Test: Trade event semantics... ";
    Engine engine(10);

    Order maker{
        1001,
        Side::Sell,
        10100,
        100,
        1,
        OrderType::Limit
    };

    Order taker{
        2001,
        Side::Buy,
        10100,
        40,
        2,
        OrderType::Limit
    };

    std::vector<TradeEvent> trades;

    engine.ProcessOrder(maker, trades);

    trades.clear();

    engine.ProcessOrder(taker, trades);

    assert(trades.size() == 1);

    const TradeEvent& trade = trades[0];

    assert(trade.makerOrderId == 1001);
    assert(trade.takerOrderId == 2001);
    assert(trade.price == 10100);
    assert(trade.quantity == 40);

    std::cout << "PASSED ✅\n";
}

void TestTradeEventPartialFill() {
    std::cout << "Running Test: Trade event partial fill... ";
    Engine engine(10);

    Order maker{
        1001,
        Side::Sell,
        10100,
        100,
        1,
        OrderType::Limit
    };

    Order taker{
        2001,
        Side::Buy,
        10100,
        40,
        2,
        OrderType::Limit
    };

    std::vector<TradeEvent> trades;

    engine.ProcessOrder(maker, trades);

    trades.clear();

    engine.ProcessOrder(taker, trades);

    assert(trades.size() == 1);

    assert(trades[0].makerOrderId == 1001);
    assert(trades[0].takerOrderId == 2001);
    assert(trades[0].price == 10100);
    assert(trades[0].quantity == 40);

    // Maker should have 60 remaining.
    auto it = engine.asks.orderMap.find(1001);

    assert(it != engine.asks.orderMap.end());
    assert(it->second->order.quantity == 60);

    std::cout << "PASSED ✅\n";
}

void TestTradeEventMultiLevelMatch() {
    std::cout << "Running Test: Trade event multi-level match... ";
    Engine engine(10);

    Order maker1{
        1001,
        Side::Sell,
        10100,
        100,
        1,
        OrderType::Limit
    };

    Order maker2{
        1002,
        Side::Sell,
        10200,
        50,
        2,
        OrderType::Limit
    };

    Order taker{
        2001,
        Side::Buy,
        10200,
        150,
        3,
        OrderType::Limit
    };

    std::vector<TradeEvent> trades;

    engine.ProcessOrder(maker1, trades);

    trades.clear();

    engine.ProcessOrder(maker2, trades);

    trades.clear();

    engine.ProcessOrder(taker, trades);

    assert(trades.size() == 2);

    // First execution: best ask.
    assert(trades[0].makerOrderId == 1001);
    assert(trades[0].takerOrderId == 2001);
    assert(trades[0].price == 10100);
    assert(trades[0].quantity == 100);

    // Second execution: next price level.
    assert(trades[1].makerOrderId == 1002);
    assert(trades[1].takerOrderId == 2001);
    assert(trades[1].price == 10200);
    assert(trades[1].quantity == 50);

    std::cout << "PASSED ✅\n";
}

void TestTradeEventFIFOMakerPriority() {
    std::cout << "Running Test: Trade event FIFO scheduling... ";
    Engine engine(10);

    Order maker1{
        1001,
        Side::Sell,
        10100,
        40,
        1,
        OrderType::Limit
    };

    Order maker2{
        1002,
        Side::Sell,
        10100,
        40,
        2,
        OrderType::Limit
    };

    Order taker{
        2001,
        Side::Buy,
        10100,
        50,
        3,
        OrderType::Limit
    };

    std::vector<TradeEvent> trades;

    engine.ProcessOrder(maker1, trades);

    trades.clear();

    engine.ProcessOrder(maker2, trades);

    trades.clear();

    engine.ProcessOrder(taker, trades);

    assert(trades.size() == 2);

    // Earlier maker gets filled first.
    assert(trades[0].makerOrderId == 1001);
    assert(trades[0].quantity == 40);

    // Remaining quantity goes to second maker.
    assert(trades[1].makerOrderId == 1002);
    assert(trades[1].quantity == 10);

    assert(trades[0].takerOrderId == 2001);
    assert(trades[1].takerOrderId == 2001);

    assert(trades[0].price == 10100);
    assert(trades[1].price == 10100);

    std::cout << "PASSED ✅\n";
}

void TestTradeEventSellTaker() {
    std::cout << "Running Test: Trade event reverse semantics... ";
    Engine engine(10);

    Order maker{
        1001,
        Side::Buy,
        10000,
        100,
        1,
        OrderType::Limit
    };

    Order taker{
        2001,
        Side::Sell,
        10000,
        40,
        2,
        OrderType::Limit
    };

    std::vector<TradeEvent> trades;

    engine.ProcessOrder(maker, trades);

    trades.clear();

    engine.ProcessOrder(taker, trades);

    assert(trades.size() == 1);

    assert(trades[0].makerOrderId == 1001);
    assert(trades[0].takerOrderId == 2001);
    assert(trades[0].price == 10000);
    assert(trades[0].quantity == 40);

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
    TestEmptyOrderBookSnapshot();
    TestOrderBookSnapshotDepth();
    TestOrderBookSnapshotLimitedDepth();
    TestOrderBookSnapshotAggregatesPriceLevel();
    TestOrderBookSnapshotZeroDepth();
    TestOrderBookSnapshotAfterCancellation();
    TestTradeEventMakerTakerSemantics();
    TestTradeEventPartialFill();
    TestTradeEventMultiLevelMatch();
    TestTradeEventFIFOMakerPriority();
    TestTradeEventSellTaker();

    std::cout << "\nALL UNIT TESTS PASSED SUCCESSFULLY! 🎉\n";
    std::cout << "========================================\n";
    return 0;
}
