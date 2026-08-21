#pragma once

#include <cstdint>

// Structure to capture trade executions without blocking on I/O
struct TradeEvent {
    uint64_t makerOrderId;
    uint64_t takerOrderId;
    uint64_t price;
    uint32_t quantity;
};