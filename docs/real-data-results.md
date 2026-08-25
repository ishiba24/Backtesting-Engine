# Real EUR/USD data experiment

## Dataset

- Provider: Dukascopy Bank SA historical data feed
- Instrument: EUR/USD
- Requested range: January 2, 2024 UTC, end-exclusive at January 3
- Actual first tick: `2024-01-02T00:00:01.340Z`
- Actual last tick: `2024-01-02T23:59:45.953Z`
- Quote updates: 101,427
- Source event precision: milliseconds
- Normalized timestamp unit: Unix nanoseconds
- Normalized CSV SHA-256:
  `8c434576b93c5cc393df768b9ecb87e32c22e029c8c9823302250f6d4766e144`

The normalized CSV is Git-ignored and reproducible from the command in
`data/README.md`. The tracked manifest is
`data/manifests/EURUSD_2024-01-02.json`.

## Buy-and-hold run

Configuration:

```text
quantity: 10,000 EUR
direction: long
leverage: 30:1
fees: 0
additional slippage: 0
```

Result:

```text
ticks: 101,427
fills: 1
final balance: $100,000.00
unrealized PnL: -$95.50
final equity: $99,904.50
total return: -0.0955%
maximum drawdown: 0.1057%
```

The position remained open, so its loss was unrealized at the final quote.

## SMA crossover run

Configuration:

```text
fast SMA: 20 ticks
slow SMA: 50 ticks
target position: 10,000 EUR long or short
stop loss: 50 basis points
take profit: 100 basis points
leverage: 30:1
fees: 0
additional slippage: 0
```

Result:

```text
ticks: 101,427
fills: 2,323
realized PnL: -$645.40
unrealized PnL: +$1.30
final equity: $99,355.90
total return: -0.6441%
maximum drawdown: 0.6465%
closed-position win rate: 22.7390%
```

No stop or target fired because crossover reversals closed positions before
those wider thresholds were reached. Zero fees isolate spread and strategy
behavior; a realistic evaluation should rerun with broker-specific fees and
slippage.

## Interpretation

These runs prove that the real-data download, normalization, fixed-point parser,
tick replay, bid/ask execution, strategy dispatch, and reporting pipeline work
together. They do not demonstrate a profitable strategy. One trading day is too
short for statistically meaningful Sharpe, volatility, or CAGR, and the SMA
periods here represent ticks rather than minutes or days.
