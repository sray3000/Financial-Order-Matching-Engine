#include <iostream>
#include <algorithm>
#include "OrderBook.cc"

class Engine {
public:    
    OrderBook<std::greater<long long>> bids;
    OrderBook<std::less<long long>> asks;

    Engine() {};

    void ProcessOrder(Order incomingOrder) {
        if (incomingOrder.type == 0) {          // Bid (buy order)
            while (incomingOrder.quantity > 0 && !asks.orderBook.empty()) {
                auto bestAsk = asks.orderBook.begin();
                long long askPrice = bestAsk->first;
                
                if (incomingOrder.price < askPrice) {
                    break; 
                }

                auto& askList = bestAsk->second;
                while (incomingOrder.quantity > 0 && !askList.empty()) {
                    auto& restingAsk = askList.front();
                    
                    long long tradeQuantity = std::min(incomingOrder.quantity, restingAsk.quantity);
                    
                    std::cout << "TRADE EXECUTED: " << tradeQuantity 
                              << " units @ $" << askPrice << "\n";
                    
                    incomingOrder.quantity -= tradeQuantity;
                    restingAsk.quantity -= tradeQuantity;
                    
                    if (restingAsk.quantity == 0) {
                        asks.orderMap.erase(restingAsk.orderId);
                        askList.pop_front();
                    }
                }

                if (askList.empty()) {
                    asks.orderBook.erase(bestAsk);
                }
            }

            if (incomingOrder.quantity > 0) {
                bids.AddOrder(incomingOrder);
            }

        } else {                    // Ask (sell order)
            while (incomingOrder.quantity > 0 && !bids.orderBook.empty()) {
                auto bestBid = bids.orderBook.begin();
                long long bidPrice = bestBid->first;
                
                if (incomingOrder.price > bidPrice) {
                    break; 
                }

                auto& bidList = bestBid->second;
                while (incomingOrder.quantity > 0 && !bidList.empty()) {
                    auto& restingBid = bidList.front();
                    
                    long long tradeQuantity = std::min(incomingOrder.quantity, restingBid.quantity);
                    
                    std::cout << "TRADE EXECUTED: " << tradeQuantity 
                              << " units @ $" << bidPrice << "\n";
                    
                    incomingOrder.quantity -= tradeQuantity;
                    restingBid.quantity -= tradeQuantity;
                    
                    if (restingBid.quantity == 0) {
                        bids.orderMap.erase(restingBid.orderId);
                        bidList.pop_front();
                    }
                }

                if (bidList.empty()) {
                    bids.orderBook.erase(bestBid);
                }
            }

            if (incomingOrder.quantity > 0) {
                asks.AddOrder(incomingOrder);
            }
        }
    }

    void PrintBook() {
        std::cout << "\t\tBIDS\t\t\t\tASKS\t\t\n";
        // To be implemented: iterate through maps and print
    }
};