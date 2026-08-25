# Real market data

Generated market data is intentionally excluded from Git because tick files can
be large and may be subject to provider terms. The project uses Dukascopy Bank's
historical bid/ask feed as its reproducible real-data source.

Download one UTC day of EUR/USD quotes:

```bash
python python/download_dukascopy.py \
  --symbol EURUSD \
  --start 2024-01-02 \
  --end 2024-01-03 \
  --output data/processed/EURUSD_2024-01-02.csv \
  --manifest data/manifests/EURUSD_2024-01-02.json
```

`--end` is exclusive. Dukascopy source ticks are timestamped to milliseconds;
the normalized engine field is Unix nanoseconds, so every source millisecond is
multiplied by 1,000,000 without inventing greater source precision.

Run both strategies:

```bash
./build/backtest --data data/processed/EURUSD_2024-01-02.csv \
  --strategy buy_and_hold --symbol EURUSD --quantity 10000

./build/backtest --data data/processed/EURUSD_2024-01-02.csv \
  --strategy sma_crossover --symbol EURUSD --quantity 10000 \
  --fast 20 --slow 50 --stop-loss-bps 50 --take-profit-bps 100
```

The tracked manifest records the source, requested period, row count, first and
last quote times, normalized-file checksum, and limitations. The normalized CSV
can be reproduced locally from the same command.
