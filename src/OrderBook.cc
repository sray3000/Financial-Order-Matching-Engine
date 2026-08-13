#pragma once
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
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
    OrderPool(size_t capacity) : pool(capacity) {
        // Link all pre-allocated nodes into a free list
        for (size_t i = 0; i < capacity - 1; ++i) {
            pool[i].next = &pool[i + 1];
        }
        freeHead = &pool[0];
    }

    OrderNode* Acquire(const Order& order) {
        if (!freeHead) {
            std::cerr << "CRITICAL ERROR: OrderPool exhausted!\n";
            return nullptr; 
        }
        OrderNode* node = freeHead;
        freeHead = freeHead->next;
        
        // Initialize node
        node->order = order;
        node->next = nullptr;
        node->prev = nullptr;
        
        return node;
    }

    void Release(OrderNode* node) {
        node->next = freeHead;
        freeHead = node;
    }
};

// 3. Define the Price Level (Intrusive Doubly-Linked List)
struct PriceLevel {
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;

    void PushBack(OrderNode* node) {
        if (!tail) {
            head = tail = node;
            node->prev = node->next = nullptr;
        } else {
            tail->next = node;
            node->prev = tail;
            node->next = nullptr;
            tail = node;
        }
    }

    void PopFront() {
        if (!head) return;
        OrderNode* oldHead = head;
        head = head->next;
        if (head) {
            head->prev = nullptr;
        } else {
            tail = nullptr;
        }
    }

    void Erase(OrderNode* node) {
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (node == head) head = node->next;
        if (node == tail) tail = node->prev;
    }

    bool IsEmpty() const {
        return head == nullptr;
    }
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

    OrderBook(OrderPool* sharedPool) : pool(sharedPool) {}

    void AddOrder(const Order& newOrder) {
        // Grab a pre-allocated node from the pool instead of using 'new'
        OrderNode* node = pool->Acquire(newOrder);
        if (!node) return;

        uint64_t price = newOrder.price;
        orderBook[price].PushBack(node);
        orderMap[newOrder.orderId] = node;
    }

    void CancelOrder(uint64_t orderId) {
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()) {
            OrderNode* node = it->second;
            uint64_t price = node->order.price;
            
            // O(1) removal from the intrusive list
            orderBook[price].Erase(node);
            
            // O(1) removal from the hash map
            orderMap.erase(it);
            
            // Return memory to the pool
            pool->Release(node);
            
            // Cleanup empty price levels
            if (orderBook[price].IsEmpty()) {
                orderBook.erase(price);
            }
        }
    }
};