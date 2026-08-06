#pragma once
#include <iostream>
#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include "Order.h"

template<typename Comparator>
class OrderBook {
public:
    // Maps prices to list of orders by time
    std::map<long long, std::list<Order>, Comparator> orderBook;
    
    // Maps orderId to its exact position in the list for O(1) cancellation
    std::unordered_map<std::string, typename std::list<Order>::iterator> orderMap;

    OrderBook() {};

    void AddOrder(const Order& newOrder) {
        long long price = newOrder.price;
        orderBook[price].push_back(newOrder);
        orderMap[newOrder.orderId] = std::prev(orderBook[price].end());
    }

    void CancelOrder(const std::string& orderId) {
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()) {
            long long price = it->second->price;
            orderBook[price].erase(it->second);
            orderMap.erase(it);
            
            if (orderBook[price].empty()) {
                orderBook.erase(price);
            }
        }
    }
};