#!/usr/bin/env python3
"""Plot an equity curve CSV produced by the C++ engine (--log-equity)."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv",
        type=Path,
        help="Path to equity_curve.csv (timestamp,equity,cash,shares)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Optional path to save the figure instead of showing it",
    )
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    # Tick timestamps are Unix nanoseconds.
    df["date"] = pd.to_datetime(df["timestamp_ns"], unit="ns", utc=True)

    plt.figure(figsize=(10, 5))
    plt.plot(df["date"], df["equity"], label="Equity")
    plt.title("Equity curve")
    plt.xlabel("Date")
    plt.ylabel("Equity (USD)")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(args.output, dpi=150)
        print(f"Wrote {args.output}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
