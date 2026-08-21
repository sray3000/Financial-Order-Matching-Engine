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
