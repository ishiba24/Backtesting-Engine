from __future__ import annotations

import lzma
import struct
from datetime import datetime, timezone

from download_dukascopy import decode_hour, hour_url, price_divisor


def test_builds_zero_based_month_url() -> None:
    hour = datetime(2024, 1, 2, 3, tzinfo=timezone.utc)
    assert hour_url("eurusd", hour).endswith(
        "/EURUSD/2024/00/02/03h_ticks.bi5"
    )


def test_decodes_bid_ask_and_millisecond_timestamp() -> None:
    hour = datetime(2024, 1, 2, 0, tzinfo=timezone.utc)
    raw = struct.pack(
        ">IIIff",
        1_340,     # milliseconds into the UTC hour
        110_370,   # ask = 1.10370
        110_366,   # bid = 1.10366
        3.6,       # ask provider volume
        0.9,       # bid provider volume
    )
    ticks = decode_hour(lzma.compress(raw), "EURUSD", hour, 100_000)
    assert len(ticks) == 1
    assert ticks[0].timestamp_ns == 1_704_153_601_340_000_000
    assert ticks[0].bid == "1.103660"
    assert ticks[0].ask == "1.103700"
    assert ticks[0].bid_size == "0.900000"
    assert ticks[0].ask_size == "3.600000"


def test_uses_jpy_price_divisor() -> None:
    assert price_divisor("EURUSD") == 100_000
    assert price_divisor("USDJPY") == 1_000
