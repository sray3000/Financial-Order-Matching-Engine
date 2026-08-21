#include <iostream>

#include "OrderBook.h"

OrderPool::OrderPool(size_t capacity) : pool(capacity) {
    // Link all pre-allocated nodes into a free list
    for (size_t i = 0; i + 1 < capacity; ++i) {
        pool[i].next = &pool[i + 1];
    }

    if(capacity > 0)
      freeHead = &pool[0];
}

OrderNode* OrderPool::Acquire(const Order& order) {
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

void OrderPool::Release(OrderNode* node) {
    node->next = freeHead;
    node->prev = nullptr;
    freeHead = node;
}

void PriceLevel::PushBack(OrderNode* node) {
    if(!tail) {
        head = tail = node;
        node->prev = nullptr;
        node->next = nullptr;
    } else {
        tail->next = node;
        node->prev = tail;
        node->next = nullptr;
        tail = node;
    }
}

void PriceLevel::PopFront() {
    if(!head) return;
    OrderNode* oldHead = head;
    head = head->next;
    if(head) {
        head->prev = nullptr;
    } else {
        tail = nullptr;
    }

    oldHead->next = nullptr;
    oldHead->prev = nullptr;
}

void PriceLevel::Erase(OrderNode* node) {
    if(node->prev) node->prev->next = node->next;
    if(node->next) node->next->prev = node->prev;
    if(node == head) head = node->next;
    if(node == tail) tail = node->prev;

    node->prev = nullptr;
    node->next = nullptr;
}

bool PriceLevel::IsEmpty() const {
    return head == nullptr;
}

// 4. The Order Book
template<typename Comparator>
OrderBook<Comparator>::OrderBook(OrderPool* sharedPool)
    : pool(sharedPool) {
    }

// Returns false when the shared pool has no capacity for another resting order.
template<typename Comparator> 
bool OrderBook<Comparator>::AddOrder(const Order& newOrder) {
    // An order ID must be unique among all active orders.
    if (orderMap.find(newOrder.orderId) != orderMap.end())
      return false;

    // Only acquire a pool node after all validation succeeds.
    OrderNode* node = pool->Acquire(newOrder);
    
    if(!node) return false;

    uint64_t price = newOrder.price;
    orderBook[price].PushBack(node);
    orderMap[newOrder.orderId] = node;
    return true;
}

template<typename Comparator> 
void OrderBook<Comparator>::CancelOrder(uint64_t orderId) {
    auto it = orderMap.find(orderId);
    if(it == orderMap.end()) 
      return;
    
    OrderNode* node = it->second;
    uint64_t price = node->order.price;
            
    // O(1) removal from the intrusive list
    orderBook[price].Erase(node);
    // O(1) removal from the hash map
    orderMap.erase(it);
    // Return memory to the pool
    pool->Release(node);
            
    // Cleanup empty price levels
    if(orderBook[price].IsEmpty()) {
        orderBook.erase(price);
    }
}

template class OrderBook<std::greater<uint64_t>>;
template class OrderBook<std::less<uint64_t>>;