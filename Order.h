#pragma once
#include <string>

struct Order {
    std::string orderId;
    bool type;              // 0 for bid, 1 for ask
    long long price;
    long long quantity;
    std::string timestamp;
};