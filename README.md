# Financial Trade Orders Matching Engine

A compact C++17, single-process financial order matching engine focused on **price-time-priority matching, exchange-style execution semantics, market microstructure analytics, and low-latency in-process benchmarking**.

The project supports limit and market orders, cancellation, partial and multi-level fills, FIFO priority, explicit maker/taker trade events, Level-1/Level-2 order-book analytics, a POSIX TCP demonstration, regression tests, and synthetic performance workloads.

> **Scope:** This is an educational low-latency trading-engine prototype. It is intentionally small and is **not an exchange-ready production system**.

## What it does

- Matches **limit orders** at the best available opposite-side price, then rests any unfilled quantity on the book.
- Matches **market orders** against available opposite-side liquidity and discards any unfilled quantity; market orders never rest.
- Enforces **price priority** across price levels and **FIFO time priority** within the same price level.
- Supports partial fills and multi-level matching.
- Supports cancellation by order ID and side for active resting orders.
- Uses a fixed-capacity object pool for resting order nodes, avoiding per-order `new` allocation on the normal resting-order path.
- Produces explicit `TradeEvent` execution reports containing **maker order ID, taker order ID, execution price, and execution quantity**.
- Exposes top-of-book market metrics including best bid/ask, spread, mid-price, and bid/ask imbalance.
- Exposes configurable **Level-2 order-book depth snapshots** with aggregate quantity at each price level.

Prices are unsigned integers in cents. For example, `10500` represents `$105.00`.

## Repository layout

| Location | Purpose |
| --- | --- |
| `include/Order.h` | `Order`, `Side`, and `OrderType` definitions. |
| `include/TradeEvent.h` | Maker/taker execution-event definition. |
| `include/MarketMetrics.h` | Top-of-book market metrics. |
| `include/OrderBookSnapshot.h` | Level-2 depth snapshot structures. |
| `include/const.h` | Default capacity and benchmark-order-count constants. |
| `src/OrderBook.cc` | Object pool, intrusive FIFO list, price levels, and `OrderBook`. |
| `src/Engine.cc` | Matching loop, trade events, cancellation interface, market metrics, and snapshots. |
| `src/main.cc` | Unified interactive program: local TCP server thread plus local client. |
| `src/server.cc`, `src/client.cc` | Earlier standalone POSIX examples; not exposed as CMake build targets. |
| `tests/test_engine.cc` | Core behavior and execution-semantics regression tests. |
| `benchmarks/benchmark.cc` | Synthetic multi-workload core-engine benchmark. |
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
                                       TradeEvent results
                                               |
                               +---------------+---------------+
                               |               |               |
                           maker ID        taker ID       price / quantity

Market-data APIs
       |
       +--> MarketMetrics (L1)
       |
       +--> OrderBookSnapshot (L2 depth)
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

Incoming sells use the mirrored process against the highest bid.

Every execution is represented by a `TradeEvent`:

```text
makerOrderId   = resting order
 takerOrderId  = incoming order
price          = resting maker price
quantity       = executed quantity
```

One incoming order may generate multiple `TradeEvent`s when it matches across multiple price levels.

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

## Market microstructure APIs

### Top-of-book metrics

`Engine::GetMarketMetrics()` exposes:

- best bid
- best ask
- quantity at best bid
- quantity at best ask
- bid-ask spread
- mid-price
- top-of-book imbalance

The imbalance is calculated as:

```text
(bid quantity - ask quantity)
--------------------------------
(bid quantity + ask quantity)
```

with floating-point conversion performed before subtraction to avoid unsigned-integer underflow.

### Level-2 depth snapshots

`Engine::GetSnapshot(depth)` exposes the first `depth` price levels on both sides:

```text
ASK
10200 x 100
10100 x 40
----------------
10000 x 30
 9900 x 100
BID
```

Bids are returned best-to-worse (descending price); asks are returned best-to-worse (ascending price). Multiple orders at the same price are aggregated into a single `BookLevel`.

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

The regression suite covers matching correctness, price-time priority, partial and multi-level fills, market-order behavior, cancellation/pool behavior, market metrics, Level-2 snapshots, and maker/taker execution semantics.

### Run the unified interactive engine

```bash
./build/matching_engine
```

Enter orders in the format shown above. The program starts its local server automatically; do not start a separate server or client target.

### Run the benchmark

```bash
./build/matching_benchmark
```

## Benchmark methodology and results

`matching_benchmark` pre-generates 1,000,000 orders per workload before timing, so order generation is outside the measured matching path. It runs a warm-up on a separate engine and uses a fresh engine for each measured workload. A caller-owned `TradeEvent` buffer is reserved before timing, keeping result-vector allocation outside each timed call. Throughput uses a single `steady_clock` interval around the matching loop; latency is measured separately per order and reported as p50, p99, and p99.9. fileciteturn3file0L11-L47

Three synthetic order-flow regimes are benchmarked:

| Workload | Purpose | Observed behavior |
| --- | --- | --- |
| **Balanced** | General matching workload with limit orders and a market-order mix | Moderate trade generation and a moderate active book |
| **Resting-heavy** | Stress persistent resting liquidity and book maintenance | No trades; approximately 1,000,000 active resting orders in the diagnostic run |
| **Aggressive** | Stress immediate matching and order removal | Approximately 500,000 trades and a minimal active book |

### Representative local run

One final local run produced:

```text
Workload         Orders/sec       Trades/order       p50       p99       p99.9
Balanced         25,120,258.84    0.49               53 ns     150 ns    425 ns
Resting-heavy    21,159,118.52    0.00               39 ns      60 ns    272 ns
Aggressive       30,143,491.76    0.50               42 ns      82 ns    211 ns
```

These are **machine- and run-dependent local measurements**. They measure the in-process matching path for synthetic workloads and do not represent network, serialization, logging, persistence, risk, or production exchange latency. The benchmark should be used primarily for before/after comparisons on the same machine and build configuration rather than as a universal throughput claim.

## Verification

The implementation was also exercised through the interactive executable with the following scenarios:

- resting limit buy
- resting limit sell
- exact match
- partial fill
- multi-level matching
- FIFO / price-time priority at the same price
- market buy across multiple levels
- market order with insufficient liquidity, confirming that unfilled market quantity does not rest

Representative execution output follows the exchange-style semantics:

```text
[TRADE EXECUTED] Maker ID: 4 | Taker ID: 7 | Price: $101 | Qty: 60
[TRADE EXECUTED] Maker ID: 6 | Taker ID: 7 | Price: $102 | Qty: 30
```

## Current scope and limitations

- The supported application mode is a single unified local session, not a multi-client exchange service.
- The engine does not implement order amendments, stop orders, iceberg orders, self-trade prevention, persistence, recovery, risk controls, external market-data feeds, or durable audit logging.
- Price-level aggregate liquidity is currently derived from the resting-order structure rather than maintained as a dedicated `O(1)` quantity field; this was intentionally left as future optimization work.
- The raw TCP protocol should be replaced with an explicit, versioned, endian-safe framed protocol before communicating across processes or hosts.
- Benchmark latency is in-process matching latency and includes timestamping overhead; it is not end-to-end exchange latency.
