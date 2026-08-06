#include<iostream>
#include<map>
#include<queue>
#include<functional>
#include<first.h>

template<typename Comparator>
class OrderBook {
public:
    std::map<long long, std::queue<Order>, Comparator> orderBook;

    OrderBook() {

    };

    void AddOrder(Order newOrder) {
        long long price = newOrder.price;
        orderBook[price].push(newOrder);
    }
};