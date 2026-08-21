#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

#include "Order.h"

// 1. Define the Intrusive Node
struct OrderNode {
    Order order;
    OrderNode* prev = nullptr;
    OrderNode* next = nullptr;
};

// 2. Define the Object Pool to eliminate runtime heap allocations
class OrderPool {
private:
    std::vector<OrderNode> pool;
    OrderNode* freeHead = nullptr;

public:
    explicit OrderPool(size_t);
    OrderNode* Acquire(const Order&);
    void Release(OrderNode*);
};

// 3. Define the Price Level (Intrusive Doubly-Linked List)
struct PriceLevel {
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;

    void PushBack(OrderNode*);
    void PopFront();
    void Erase(OrderNode*);
    bool IsEmpty() const;
};

// 4. The Order Book
template<typename Comparator>
class OrderBook {
public:
    // Maps prices to our custom intrusive list
    std::map<uint64_t, PriceLevel, Comparator> orderBook;
    // Maps orderId directly to the memory pointer for pure O(1) lookups
    std::unordered_map<uint64_t, OrderNode*> orderMap;
    // Shared memory pool pointer (usually owned by the Engine)
    OrderPool* pool;

    explicit OrderBook(OrderPool*);
    // Returns false when the shared pool has no capacity for another resting order.
    [[nodiscard]] bool AddOrder(const Order&);
    void CancelOrder(uint64_t);
};