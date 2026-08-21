#pragma once

#include <cstdint>

// Represents a single executed trade between a resting maker
// order and an incoming taker order.
//
// Execution price is always the resting order's price.
// One incoming order may generate multiple TradeEvents when
// it matches across multiple price levels.
struct TradeEvent {
    uint64_t makerOrderId;
    uint64_t takerOrderId;
    uint64_t price;
    uint32_t quantity;
};