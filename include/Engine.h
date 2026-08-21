#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "TradeEvent.h"
#include "const.h"
#include "MarketMetrics.h"
#include "OrderBookSnapshot.h"

class Engine {
private:
    OrderPool pool;

public:
    OrderBook<std::greater<uint64_t>> bids;
    OrderBook<std::less<uint64_t>> asks;

    explicit Engine(size_t maxOrders = MAX_ORDERS);
    void ProcessOrder(Order, std::vector<TradeEvent>&);
    std::vector<TradeEvent> ProcessOrder(Order);
    void CancelOrder(uint64_t, Side);
    MarketMetrics GetMarketMetrics() const;
    OrderBookSnapshot GetSnapshot(size_t) const;
};