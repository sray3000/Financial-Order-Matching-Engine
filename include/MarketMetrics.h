#pragma once

#include <cstdint>

struct MarketMetrics {
    uint64_t bestBid = 0;
    uint64_t bestAsk = 0;
    uint64_t bidQuantity = 0;
    uint64_t askQuantity = 0;
    uint64_t spread = 0;
    
    double midPrice = 0.0;
    double imbalance = 0.0;

    bool hasBid = false;
    bool hasAsk = false;
};