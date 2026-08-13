# Financial Trade Orders Matching Engine

A compact C++17, single-process financial order matching engine. It maintains a price-time-priority limit order book, accepts limit and market orders, and includes a POSIX TCP demonstration, unit tests, and a synthetic benchmark.

This is an educational low-latency engine prototype. It is intentionally small and is not an exchange-ready system.

## What it does

- Matches **limit orders** at the best available opposite-side price, then rests any unfilled quantity on the book.
- Matches **market orders** against all available opposite-side liquidity and discards any unfilled quantity; market orders never rest.
- Enforces price priority across price levels and FIFO time priority within the same price level.
- Supports cancellation by order ID and side for active resting orders.
- Uses a fixed-capacity object pool for resting order nodes, avoiding per-order `new` allocation on the normal resting-order path.

Prices are unsigned integers in cents. For example, `10500` represents `$105.00`.

## Repository layout

| Location | Purpose |
| --- | --- |
| `include/Order.h` | `Order`, `Side`, and `OrderType` definitions. |
| `include/const.h` | Default capacity and benchmark-order-count constants. |
| `src/OrderBook.cc` | Object pool, intrusive FIFO list, price level, and `OrderBook`. |
| `src/Engine.cc` | Matching loop, trade events, and cancellation interface. |
| `src/main.cc` | Unified interactive program: local TCP server thread plus local client. |
| `src/benchmark.cc` | Synthetic core-engine benchmark. |
| `src/server.cc`, `src/client.cc` | Earlier standalone POSIX examples; not exposed as CMake build targets. |
| `tests/test_engine.cc` | Core behavior regression tests. |
| `CMakeLists.txt` | Build configuration for the unified executable, benchmark, and tests. |

## Architecture

```text
Interactive input
       |
       v
local TCP client ---- raw Order bytes ----> POSIX TCP server thread
                                                |
                                                v
                                           Engine::ProcessOrder
                                         /                       \
                                        v                         v
                              bid book (descending)       ask book (ascending)
                              highest price first          lowest price first
                                        \                         /
                                         v                       v
                                    TradeEvent results sent to client
```

The supported executable is one unified local session: `matching_engine` starts its server thread internally and connects its own interactive client to `127.0.0.1:8080`.

## Order-book data structures

Each side owns an `OrderBook`:

- `std::map<uint64_t, PriceLevel>` stores price levels. Bids use `std::greater`, so `begin()` is the highest bid. Asks use `std::less`, so `begin()` is the lowest ask.
- A `PriceLevel` is an intrusive doubly linked FIFO queue of `OrderNode`s. The head is always the oldest order at that price.
- `std::unordered_map<uint64_t, OrderNode*>` maps an active order ID to its node for direct cancellation lookup.
- `OrderPool` preallocates `OrderNode`s in a free list. When the pool is full, `AddOrder` returns `false` and the engine reports that the resting order was rejected.

The engine owns the pool and shares it between bid and ask books, so the configured capacity is a total active-resting-order limit across both sides.

## Execution logic

For an incoming buy:

1. Read the lowest ask.
2. A limit buy stops if its limit price is below that ask; a market buy has no price check.
3. Fill the oldest order at that price level first.
4. Continue until the incoming quantity is exhausted, no executable liquidity remains, or (for a limit order) the price no longer crosses.
5. Rest a remaining limit buy on the bid book. Discard a remaining market-buy quantity.

Incoming sells use the mirrored process against the highest bid. Every trade executes at the resting maker's price and produces a `TradeEvent` containing maker ID, taker ID, price, and quantity.

### Interactive order format

```text
[side] [type] [price-in-cents] [quantity]
```

`side`: `0` = buy, `1` = sell  
`type`: `0` = limit, `1` = market

Examples:

```text
1 0 10500 10    # limit sell: 10 @ $105.00
0 0 10500 4     # limit buy: 4 @ $105.00
0 1 0 4         # market buy: up to 4 at available asks
```

The `price` field of a market order is ignored; use `0` for clarity.

## Complexity

Let `P` be the number of active price levels, `F` the number of resting orders fully consumed by an incoming order, `N` the maximum pool capacity, and `K` the number of emitted trade events.

| Operation | Complexity | Notes |
| --- | --- | --- |
| Add a resting order | `O(log P)` average | Price-level map insertion/lookup; FIFO append and ID-map insert are average `O(1)`. |
| Find best bid/ask | `O(1)` | `std::map::begin()` is the best price because of each book's comparator. |
| Cancel an order | `O(log P)` average | ID lookup and intrusive-list unlink are average `O(1)`; price-level lookup/possible erase costs `O(log P)`. |
| Match an order | `O(F log P + K)` | It walks price levels and FIFO orders; completed makers require book cleanup. |
| Resting-book memory | `O(N + P)` | Pool capacity is fixed at `N`; price-level map uses `O(P)`. |

## POSIX TCP support

The unified program demonstrates a POSIX TCP path using `socket`, `bind`, `listen`, `accept`, nonblocking sockets, and Linux `epoll` with edge-triggered events. It binds to port `8080` and the built-in client connects locally.

Orders and trade events are currently transmitted as raw in-memory C++ structs. This is convenient for the local demonstration, but it is **not a portable production wire protocol**: struct layout/padding, byte order, partial reads/writes, framing, reconnect handling, authentication, and backpressure are not fully handled. Keep the TCP component on Linux/WSL and treat it as a prototype.

## Build and run

### Requirements

- Linux or WSL (the interactive TCP target uses POSIX sockets and `epoll`)
- CMake 3.15+
- A C++17 compiler, such as GCC or Clang

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`CMakeLists.txt` enables `-O3`, `-march=native`, `-Wall`, and `-Wextra`. `-march=native` optimizes for the build machine, so rebuild on the machine where you will run the binaries.

### Run the tests

```bash
./build/run_tests
```

### Run the unified interactive engine

```bash
./build/matching_engine
```

Enter orders in the format shown above. The program starts its local server automatically; do not start a separate server or client target.

### Run the benchmark

```bash
./build/matching_benchmark
```

## Benchmark workload and sample result

`matching_benchmark` pre-generates `NUM_ORDERS` (currently 1,000,000) orders before timing. It uses quantity `10`, alternating sides, limit prices around `$100.00`, and a balanced 10% market-order mix (one market buy and one market sell per twenty orders). Warm-up runs on a separate engine, and the measured run starts with a fresh engine. A caller-owned `TradeEvent` buffer is reserved before timing, so result-vector allocation is outside the timed matching path.

One optimized local run with MSYS2 MinGW `g++ 15.1.0` produced:

```text
Orders processed : 1,000,000
Elapsed time     : 133 ms
Throughput       : 7,518,796 orders/sec
Average latency  : 97.0 ns
p50              : 100 ns
p99              : 300 ns
p99.9            : 1,800 ns
```

These numbers are machine- and run-dependent. They measure the in-process matching path for this synthetic workload, and include clock-reading overhead; they do not represent network, serialization, logging, persistence, or multi-user exchange latency. Use repeated runs on an otherwise idle machine to compare changes.

## Current scope and limitations

- The supported application mode is a single unified local session, not a multi-client exchange service.
- The engine does not yet implement order amendments, stop orders, iceberg orders, self-trade prevention, persistence, recovery, risk controls, market data feeds, or durable audit logging.
- The core types are currently included from `.cc` files for simplicity. A production refactor should separate declarations into headers and compile implementation units normally.
- The raw TCP protocol should be replaced with an explicit, versioned, endian-safe framed protocol before communicating across processes or hosts.
