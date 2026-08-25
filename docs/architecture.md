# Architecture and interview notes

## Design goals

The engine separates market-data ingestion, strategy decisions, execution,
accounting, and reporting. Its main correctness invariant is that a strategy
decision made from quote `i` executes no earlier than the next quote for that
instrument. Protective orders are different: once their threshold is observed,
they execute against the currently executable bid or ask.

## Data flow

1. Python downloads official Dukascopy hourly BI5 files or validates an existing
   vendor quote CSV.
2. C++ parses exact decimal fields into a contiguous `std::vector<Tick>`.
3. `InstrumentRegistry` resolves symbols through `std::unordered_map` once and
   assigns compact integer IDs.
4. The engine receives a C++20 `std::span<const Tick>`, so full datasets and
   date slices use one API without copying.
5. ID-indexed quote and pending-order vectors keep string hashing out of the
   per-tick path.
6. `Account` holds only sparse open/previous positions in an unordered map.
7. Metrics and writers consume the completed run result.

## Fixed-point representation

`Decimal` stores a signed 64-bit integer scaled by 1,000,000. For example,
`1.100200` is stored as `1,100,200`. Parsing is exact, multiplication uses a
wide intermediate, and values with more than six fractional digits are rejected.

This avoids binary floating-point artifacts in fills and PnL. The trade-off is
a bounded range, six-digit precision, and extra overflow/scale handling.

## Execution and accounting

For position quantity `q`, fill price `p`, contract multiplier `c`, and static
quote-to-account conversion `x`:

```text
notional = abs(q) * p * c * x
margin   = notional / leverage
```

Long quantity is positive and short quantity is negative. Closing PnL is:

```text
realized = closed_quantity * (exit - entry) * c * x * direction
direction = +1 for a long, -1 for a short
```

Unrealized PnL uses bid to liquidate a long and ask to cover a short. Fees are
deducted from balance independently, so `equity = balance + unrealized_pnl`.
Adds update average entry by quantity; reductions retain entry; a reversal
realizes the old side and starts the remainder at the reversal fill.

## Risk orders

Stop-loss and take-profit distances are configured in basis points from average
entry. Long thresholds observe bid; short thresholds observe ask. If the market
gaps through a threshold, the model fills at the observed executable quote,
not at the threshold, which avoids granting an impossible price.

## C++20 usage

The central `run` function accepts `std::span<const Tick>`. A span is a small,
non-owning pointer-and-length view. It lets the loader's vector, a test array,
or a date slice call the same function with no data copy. Before C++20, the API
would normally expose iterator pairs, a custom view type, or a templated range;
iterator pairs are easier to mismatch and a template would push implementation
into headers. The project also uses C++20 designated initializers, defaulted
three-way comparison, and `unordered_map::contains`.

## Complexity and performance

- CSV load: O(n)
- Date slice: O(log n), returned as a zero-copy span
- Replay: O(n + fills) for the supplied strategies
- Rolling SMA update: O(1) time and O(period) memory
- Tick storage and quote caches: contiguous vectors
- Symbol resolution and sparse positions: average O(1) hash lookup

`bench_tick_engine` constructs deterministic quotes, disables result logging,
replays the same workload repeatedly, and reports ticks/second and ns/tick. It
is a C++ baseline, not a C# comparison.

## Deliberate boundaries

- Market orders only; no persistent limit-order book
- Quote sizes are loaded but do not yet constrain fills
- Static currency conversion rather than synchronized conversion-cross ticks
- Fixed configured leverage; no broker-specific tiering or liquidation engine
- No overnight funding, swap, or short borrow cost
- In-memory replay rather than memory-mapped/binary data
- The current tracked real-data experiment covers one full UTC day, which
  validates ingestion and execution but is not enough for statistical strategy
  conclusions

These constraints should be stated directly in an interview. They define the
execution model rather than invalidating the implemented bid/ask and PnL logic.
