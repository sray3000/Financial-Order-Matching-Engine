#pragma once

#include <cstdint>
#include <vector>

struct BookLevel {
    uint64_t price = 0;
    uint64_t quantity = 0;
};

struct OrderBookSnapshot {
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
};