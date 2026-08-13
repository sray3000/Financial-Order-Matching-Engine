#pragma once
#include <cstdint>

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

struct Order {
    uint64_t orderId;
    Side side;
    uint64_t price;     // Store as cents
    uint32_t quantity;
    uint64_t timestamp; // Unix nanoseconds
    OrderType type = OrderType::Limit;
};
