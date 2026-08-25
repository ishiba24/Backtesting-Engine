# FX Tick Backtesting Engine

A C++20 event-driven backtesting engine that replays historical FX bid/ask
quotes, executes long and short strategies without look-ahead, applies margin
and transaction costs, and reports exact fixed-point PnL.

## Implemented capabilities

- Nanosecond quote ticks: `timestamp_ns,symbol,bid,ask,bid_size,ask_size`
- Multi-instrument registry with string-to-ID lookup and ID-indexed hot-loop state
- Buy-at-ask and sell-at-bid market execution with optional extra slippage
- One-quote execution delay between a strategy decision and its fill
- Signed long/short positions, partial closes, adds, and position reversals
- Volume-weighted average entry, realized PnL, executable-price unrealized PnL
- Configurable leverage, margin checks, fees, minimum fees, stop loss, and take profit
- Buy-and-hold and O(1) rolling-SMA crossover strategies
- Equity/trade CSV, summary JSON, daily risk metrics, and a throughput benchmark
- Six-decimal fixed-point `Decimal` values instead of binary floating-point prices

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For a meaningful performance baseline:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/bench_tick_engine 1000000 5
```

## Real input data

The reproducible sample uses Dukascopy Bank's historical bid/ask feed. The
tracked [data manifest](data/manifests/EURUSD_2024-01-02.json) describes 101,427
real EUR/USD updates from `2024-01-02T00:00:01.340Z` through
`2024-01-02T23:59:45.953Z` and records the normalized CSV checksum.

Dukascopy source events have millisecond precision. The engine schema stores
Unix nanoseconds, so the downloader multiplies source milliseconds by 1,000,000
for a consistent unit without inventing greater timing precision.

```csv
timestamp_ns,symbol,bid,ask,bid_size,ask_size
1724025600000000000,EURUSD,1.100000,1.100200,1000000,1000000
1724025600001000000,EURUSD,1.100100,1.100300,1000000,1000000
```

Prices and sizes may have at most six fractional digits. Timestamps must be
globally ordered, and timestamps for an individual instrument must be unique.
Use `python/prepare_data.py` to validate an existing vendor CSV, or download
real Dukascopy bid/ask history with `python/download_dukascopy.py`. See
[`data/README.md`](data/README.md) for a reproducible real-data example.

See [docs/real-data-results.md](docs/real-data-results.md) for the actual
buy-and-hold and SMA runs over the tracked sample period.

## Example

```bash
./build/backtest \
  --data tests/fixtures/eurusd_ticks.csv \
  --strategy buy_and_hold \
  --symbol EURUSD \
  --quantity 10000 \
  --direction long \
  --leverage 30 \
  --stop-loss-bps 100 \
  --take-profit-bps 200 \
  --log-equity output/equity.csv \
  --log-trades output/trades.csv \
  --summary output/summary.json
```

SMA example:

```bash
./build/backtest --data ticks.csv --strategy sma_crossover \
  --symbol EURUSD --quantity 10000 --fast 20 --slow 50
```

## Execution semantics

On quote `T[i]`, the engine first executes orders generated on the previous
quote for that instrument. It then evaluates stop/target triggers and finally
invokes the strategy. New strategy orders cannot fill until `T[i+1]`.

- Buy market order: current ask plus configured slippage
- Sell market order: current bid minus configured slippage
- Long liquidation/stop/target: evaluated against bid
- Short cover/stop/target: evaluated against ask

Quote sizes are retained for future liquidity/partial-fill modeling but are not
currently used to cap fills. Currency conversion is a configurable static
quote-to-account rate rather than a replayed conversion cross.

See [docs/architecture.md](docs/architecture.md) for the full design and formulas.
