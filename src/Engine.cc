#include <iostream>
#include <algorithm>

#include "Engine.h"

// Initialize engine with a fixed pool capacity
Engine::Engine(size_t maxOrders) 
    : pool(maxOrders), bids(&pool), asks(&pool) {}

// Process an incoming order into caller-owned storage. Reusing this buffer
// avoids allocating trade-result storage on the matching path.
void Engine::ProcessOrder(Order incomingOrder, std::vector<TradeEvent>& trades) {
    trades.clear();
    // An order with no executable quantity must never enter the book.
    if(incomingOrder.quantity == 0)
      return;

    if(incomingOrder.side == Side::Buy) {
        // Match against Asks (lowest ask price first)
        while(incomingOrder.quantity > 0 && !asks.orderBook.empty()) {
            auto bestAskIt = asks.orderBook.begin();
            uint64_t askPrice = bestAskIt->first;

            // A limit bid stops below the best ask. A market order has no
            // price constraint and consumes available asks.
            if(incomingOrder.type == OrderType::Limit && incomingOrder.price < askPrice)
              break;

            PriceLevel& askLevel = bestAskIt->second;
            while(incomingOrder.quantity > 0 && !askLevel.IsEmpty()) {
                OrderNode* restingNode = askLevel.head;
                    
                uint32_t tradeQuantity = std::min(
                    static_cast<uint32_t>(incomingOrder.quantity), 
                    restingNode->order.quantity
                );

                // Record the execution event
                trades.push_back({restingNode->order.orderId, incomingOrder.orderId, askPrice, tradeQuantity});

                // Decrement quantities
                incomingOrder.quantity -= tradeQuantity;
                restingNode->order.quantity -= tradeQuantity;

                // If resting order is fully filled, remove it via CancelOrder (handles cleanup & pool release)
                if(restingNode->order.quantity == 0) {
                    uint64_t filledId = restingNode->order.orderId;
                    asks.CancelOrder(filledId);
                }
            }
        }

        // Only unfilled limit orders may rest on the book.
        if(incomingOrder.quantity > 0 && incomingOrder.type == OrderType::Limit) {
            if(!bids.AddOrder(incomingOrder))
              std::cerr << "Order rejected: insufficient pool capacity.\n";
        }
    } else {
        // Match against Bids (highest bid price first)
        while(incomingOrder.quantity > 0 && !bids.orderBook.empty()) {
            auto bestBidIt = bids.orderBook.begin();
            uint64_t bidPrice = bestBidIt->first;

            // A limit ask stops above the best bid. A market order has no
            // price constraint and consumes available bids.
            if(incomingOrder.type == OrderType::Limit && incomingOrder.price > bidPrice)
              break;

            PriceLevel& bidLevel = bestBidIt->second;
            while (incomingOrder.quantity > 0 && !bidLevel.IsEmpty()) {
                OrderNode* restingNode = bidLevel.head;
                    
                uint32_t tradeQuantity = std::min(
                    static_cast<uint32_t>(incomingOrder.quantity), 
                    restingNode->order.quantity
                );

                // Record the execution event
                trades.push_back({restingNode->order.orderId, incomingOrder.orderId, bidPrice, tradeQuantity});

                // Decrement quantities
                incomingOrder.quantity -= tradeQuantity;
                restingNode->order.quantity -= tradeQuantity;

                // If resting order is fully filled, remove it via CancelOrder
                if(restingNode->order.quantity == 0) {
                    uint64_t filledId = restingNode->order.orderId;
                    bids.CancelOrder(filledId);
                }
            }
        }

        // Only unfilled limit orders may rest on the book.
        if (incomingOrder.quantity > 0 && incomingOrder.type == OrderType::Limit) {
            if (!asks.AddOrder(incomingOrder))
              std::cerr << "Order rejected: insufficient pool capacity.\n";
        }
    }
}

// Convenience interface for callers that do not provide reusable storage.
std::vector<TradeEvent> Engine::ProcessOrder(Order incomingOrder) {
    std::vector<TradeEvent> trades;
    ProcessOrder(incomingOrder, trades);
    return trades;
}

// Direct interface to cancel an order on the correct book
void Engine::CancelOrder(uint64_t orderId, Side side) {
    if (side == Side::Buy)
      bids.CancelOrder(orderId);
    else
      asks.CancelOrder(orderId);
}

MarketMetrics Engine::GetMarketMetrics() const {
    MarketMetrics metrics;

    // Best bid
    if (!bids.orderBook.empty()) {
        metrics.hasBid = true;

        auto bidIt = bids.orderBook.begin();
        metrics.bestBid = bidIt->first;

        const PriceLevel& level = bidIt->second;

        for (OrderNode* node = level.head;
             node != nullptr;
             node = node->next) {
            metrics.bidQuantity += node->order.quantity;
        }
    }

    // Best ask
    if (!asks.orderBook.empty()) {
        metrics.hasAsk = true;

        auto askIt = asks.orderBook.begin();
        metrics.bestAsk = askIt->first;

        const PriceLevel& level = askIt->second;

        for (OrderNode* node = level.head;
             node != nullptr;
             node = node->next) {
            metrics.askQuantity += node->order.quantity;
        }
    }

    // Spread and mid-price only make sense when both sides exist.
    if (metrics.hasBid && metrics.hasAsk) {
        metrics.spread = metrics.bestAsk - metrics.bestBid;

        metrics.midPrice =
            (static_cast<double>(metrics.bestBid) +
             static_cast<double>(metrics.bestAsk)) / 2.0;
    }

    // Top-of-book imbalance.
    const uint64_t totalQuantity =
        metrics.bidQuantity + metrics.askQuantity;

    if (totalQuantity > 0) {
        metrics.imbalance =
            (static_cast<double>(metrics.bidQuantity) -
            static_cast<double>(metrics.askQuantity))
            / static_cast<double>(totalQuantity);
    }

    return metrics;
}

OrderBookSnapshot Engine::GetSnapshot(size_t depth) const {
    OrderBookSnapshot snapshot;

    if (depth == 0) {
        return snapshot;
    }

    snapshot.bids.reserve(depth);
    snapshot.asks.reserve(depth);

    // Bids are stored in descending price order.
    size_t count = 0;

    for (auto it = bids.orderBook.begin();
         it != bids.orderBook.end() && count < depth;
         ++it, ++count) {

        const uint64_t price = it->first;
        const PriceLevel& level = it->second;

        uint64_t quantity = 0;

        for (OrderNode* node = level.head;
             node != nullptr;
             node = node->next) {
            quantity += node->order.quantity;
        }

        snapshot.bids.push_back({
            price,
            quantity
        });
    }

    // Asks are stored in ascending price order.
    count = 0;

    for (auto it = asks.orderBook.begin();
         it != asks.orderBook.end() && count < depth;
         ++it, ++count) {

        const uint64_t price = it->first;
        const PriceLevel& level = it->second;

        uint64_t quantity = 0;

        for (OrderNode* node = level.head;
             node != nullptr;
             node = node->next) {
            quantity += node->order.quantity;
        }

        snapshot.asks.push_back({
            price,
            quantity
        });
    }

    return snapshot;
}