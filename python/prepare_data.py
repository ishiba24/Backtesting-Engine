"""Normalize vendor FX quote data for the C++ tick replay engine."""

from __future__ import annotations

import argparse
from pathlib import Path

import pandas as pd

OUTPUT_COLUMNS = [
    "timestamp_ns",
    "symbol",
    "bid",
    "ask",
    "bid_size",
    "ask_size",
]


def transform(frame: pd.DataFrame) -> pd.DataFrame:
    """Validate, sort, and normalize a vendor quote frame.

    Input must already provide the canonical columns. `timestamp_ns` is Unix
    nanoseconds; bid/ask prices are emitted to six decimal places to match the
    engine's fixed-point scale.
    """
    missing = set(OUTPUT_COLUMNS) - set(frame.columns)
    if missing:
        raise ValueError(f"missing columns: {sorted(missing)}")

    output = frame[OUTPUT_COLUMNS].copy().dropna()
    output["symbol"] = output["symbol"].astype(str).str.upper().str.strip()
    output["timestamp_ns"] = pd.to_numeric(
        output["timestamp_ns"], errors="raise"
    ).astype("int64")
    for column in ["bid", "ask", "bid_size", "ask_size"]:
        output[column] = pd.to_numeric(output[column], errors="raise")

    if (output["symbol"].str.len() != 6).any():
        raise ValueError("FX symbols must be six characters, e.g. EURUSD")
    if (output["timestamp_ns"] < 0).any():
        raise ValueError("timestamps cannot be negative")
    if (output["bid"] <= 0).any() or (output["ask"] < output["bid"]).any():
        raise ValueError("quotes require 0 < bid <= ask")
    if (output[["bid_size", "ask_size"]] < 0).any().any():
        raise ValueError("quote sizes cannot be negative")

    output = output.sort_values(["timestamp_ns", "symbol"], kind="stable")
    duplicates = output.duplicated(["symbol", "timestamp_ns"])
    if duplicates.any():
        raise ValueError("each instrument must have unique timestamps")
    return output.reset_index(drop=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Vendor quote CSV")
    parser.add_argument("output", type=Path, help="Normalized engine CSV")
    args = parser.parse_args()

    result = transform(pd.read_csv(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result.to_csv(args.output, index=False, float_format="%.6f")
    print(f"Wrote {len(result):,} ticks to {args.output}")


if __name__ == "__main__":
    main()
