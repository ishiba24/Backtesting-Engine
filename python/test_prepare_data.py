from __future__ import annotations

import pandas as pd
import pytest

from prepare_data import OUTPUT_COLUMNS, transform


def frame() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "timestamp_ns": [3, 1, 2],
            "symbol": ["eurusd", "eurusd", "eurusd"],
            "bid": [1.1002, 1.1000, 1.1001],
            "ask": [1.1004, 1.1002, 1.1003],
            "bid_size": [100, 100, 100],
            "ask_size": [200, 200, 200],
        }
    )


def test_normalizes_schema_symbol_and_order() -> None:
    output = transform(frame())
    assert list(output.columns) == OUTPUT_COLUMNS
    assert output["timestamp_ns"].tolist() == [1, 2, 3]
    assert output["symbol"].tolist() == ["EURUSD"] * 3


def test_rejects_crossed_quote() -> None:
    source = frame()
    source.loc[0, "ask"] = 1.0
    with pytest.raises(ValueError, match="bid <= ask"):
        transform(source)


def test_rejects_duplicate_instrument_timestamp() -> None:
    source = pd.concat([frame(), frame().iloc[[0]]], ignore_index=True)
    with pytest.raises(ValueError, match="unique timestamps"):
        transform(source)


def test_rejects_missing_column() -> None:
    with pytest.raises(ValueError, match="missing columns"):
        transform(frame().drop(columns=["ask_size"]))
