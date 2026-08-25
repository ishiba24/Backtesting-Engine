"""Download real Dukascopy FX bid/ask ticks and write the engine CSV schema.

Dukascopy stores one LZMA-compressed BI5 file per instrument/hour. Source tick
timestamps are milliseconds from the beginning of that UTC hour. The engine
schema uses Unix nanoseconds, so the downloader converts milliseconds to
nanoseconds without claiming additional source precision.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import lzma
import ssl
import struct
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from decimal import Decimal
from pathlib import Path

import certifi

BASE_URL = "https://datafeed.dukascopy.com/datafeed"
RECORD = struct.Struct(">IIIff")  # ms, ask integer, bid integer, ask volume, bid volume
TLS_CONTEXT = ssl.create_default_context(cafile=certifi.where())


@dataclass(frozen=True)
class NormalizedTick:
    timestamp_ns: int
    symbol: str
    bid: str
    ask: str
    bid_size: str
    ask_size: str


@dataclass(frozen=True)
class HourResult:
    hour: datetime
    url: str
    compressed_bytes: int
    ticks: tuple[NormalizedTick, ...]


def price_divisor(symbol: str) -> int:
    """Dukascopy encodes JPY pairs to 3 decimals and most FX pairs to 5."""
    return 1_000 if symbol.upper().endswith("JPY") else 100_000


def hour_url(symbol: str, hour: datetime) -> str:
    """Build Dukascopy's zero-based-month BI5 URL for a UTC hour."""
    hour = hour.astimezone(timezone.utc)
    return (
        f"{BASE_URL}/{symbol.upper()}/{hour.year:04d}/{hour.month - 1:02d}/"
        f"{hour.day:02d}/{hour.hour:02d}h_ticks.bi5"
    )


def decode_hour(
    payload: bytes, symbol: str, hour: datetime, divisor: int
) -> tuple[NormalizedTick, ...]:
    """Decode one BI5 payload into exact six-decimal engine records."""
    if not payload:
        return ()
    raw = lzma.decompress(payload)
    if len(raw) % RECORD.size != 0:
        raise ValueError(f"corrupt BI5 record length: {len(raw)}")

    hour_start_ns = int(hour.timestamp()) * 1_000_000_000
    result: list[NormalizedTick] = []
    previous_timestamp = -1
    decimal_divisor = Decimal(divisor)
    for millisecond, ask_integer, bid_integer, ask_volume, bid_volume in RECORD.iter_unpack(raw):
        timestamp_ns = hour_start_ns + millisecond * 1_000_000
        if timestamp_ns <= previous_timestamp:
            raise ValueError("Dukascopy ticks are not strictly ordered within the hour")
        if bid_integer <= 0 or ask_integer < bid_integer:
            raise ValueError("Dukascopy payload contains an invalid bid/ask quote")
        previous_timestamp = timestamp_ns
        result.append(
            NormalizedTick(
                timestamp_ns=timestamp_ns,
                symbol=symbol.upper(),
                bid=f"{Decimal(bid_integer) / decimal_divisor:.6f}",
                ask=f"{Decimal(ask_integer) / decimal_divisor:.6f}",
                # Dukascopy volume fields are provider-specific reported sizes.
                # The current exchange stores them but does not cap fills by size.
                bid_size=f"{bid_volume:.6f}",
                ask_size=f"{ask_volume:.6f}",
            )
        )
    return tuple(result)


def download_bytes(url: str, timeout: float, retries: int) -> bytes:
    request = urllib.request.Request(
        url, headers={"User-Agent": "Backtesting-Engine/1.0 educational-research"}
    )
    for attempt in range(retries + 1):
        try:
            with urllib.request.urlopen(
                request, timeout=timeout, context=TLS_CONTEXT
            ) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return b""
            if attempt == retries:
                raise
        except (urllib.error.URLError, TimeoutError):
            if attempt == retries:
                raise
        time.sleep(0.5 * (2**attempt))
    raise RuntimeError("unreachable download retry state")


def fetch_hour(
    symbol: str, hour: datetime, divisor: int, timeout: float, retries: int
) -> HourResult:
    url = hour_url(symbol, hour)
    payload = download_bytes(url, timeout=timeout, retries=retries)
    return HourResult(
        hour=hour,
        url=url,
        compressed_bytes=len(payload),
        ticks=decode_hour(payload, symbol, hour, divisor),
    )


def utc_hours(start: date, end: date) -> list[datetime]:
    if end <= start:
        raise ValueError("end date must be after start date")
    current = datetime(start.year, start.month, start.day, tzinfo=timezone.utc)
    finish = datetime(end.year, end.month, end.day, tzinfo=timezone.utc)
    hours: list[datetime] = []
    while current < finish:
        hours.append(current)
        current += timedelta(hours=1)
    return hours


def download_range(
    symbol: str,
    start: date,
    end: date,
    *,
    workers: int = 4,
    timeout: float = 30.0,
    retries: int = 2,
) -> list[HourResult]:
    symbol = symbol.upper().replace("/", "")
    if len(symbol) != 6 or not symbol.isalpha():
        raise ValueError("symbol must be a six-letter FX pair such as EURUSD")
    hours = utc_hours(start, end)
    divisor = price_divisor(symbol)
    results: list[HourResult] = []
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {
            executor.submit(fetch_hour, symbol, hour, divisor, timeout, retries): hour
            for hour in hours
        }
        for future in as_completed(futures):
            results.append(future.result())
    results.sort(key=lambda item: item.hour)
    return results


def write_csv(path: Path, hours: list[HourResult]) -> tuple[int, str]:
    path.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    rows = 0
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        header = ["timestamp_ns", "symbol", "bid", "ask", "bid_size", "ask_size"]
        writer.writerow(header)
        digest.update((",".join(header) + "\n").encode())
        for hour in hours:
            for tick in hour.ticks:
                row = [
                    str(tick.timestamp_ns),
                    tick.symbol,
                    tick.bid,
                    tick.ask,
                    tick.bid_size,
                    tick.ask_size,
                ]
                line = ",".join(row) + "\n"
                handle.write(line)
                digest.update(line.encode())
                rows += 1
    return rows, digest.hexdigest()


def write_manifest(
    path: Path,
    *,
    symbol: str,
    start: date,
    end: date,
    hours: list[HourResult],
    csv_path: Path,
    rows: int,
    sha256: str,
) -> None:
    nonempty = [result for result in hours if result.ticks]
    first_tick = nonempty[0].ticks[0] if nonempty else None
    last_tick = nonempty[-1].ticks[-1] if nonempty else None

    def iso(timestamp_ns: int | None) -> str | None:
        if timestamp_ns is None:
            return None
        return datetime.fromtimestamp(
            timestamp_ns / 1_000_000_000, tz=timezone.utc
        ).isoformat()

    manifest = {
        "provider": "Dukascopy Bank SA historical data feed",
        "provider_page": "https://www.dukascopy.com/swiss/english/marketwatch/historical/",
        "endpoint_template": f"{BASE_URL}/{{SYMBOL}}/{{YYYY}}/{{MM_ZERO_BASED}}/{{DD}}/{{HH}}h_ticks.bi5",
        "symbol": symbol.upper(),
        "requested_start_utc": start.isoformat(),
        "requested_end_utc_exclusive": end.isoformat(),
        "source_timestamp_precision": "milliseconds",
        "normalized_timestamp_unit": "nanoseconds",
        "rows": rows,
        "first_tick_utc": iso(first_tick.timestamp_ns if first_tick else None),
        "last_tick_utc": iso(last_tick.timestamp_ns if last_tick else None),
        "hours_requested": len(hours),
        "hours_with_ticks": len(nonempty),
        "compressed_bytes_downloaded": sum(item.compressed_bytes for item in hours),
        "normalized_csv": str(csv_path),
        "normalized_csv_sha256": sha256,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "notes": [
            "Prices are Dukascopy bid/ask quotes, not reconstructed midpoint data.",
            "Provider volume fields are preserved but the current engine does not cap fills by size.",
            "The generated CSV is ignored by Git; rerun this downloader to reproduce it.",
        ],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def parse_date(value: str) -> date:
    return date.fromisoformat(value)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--symbol", default="EURUSD")
    parser.add_argument("--start", type=parse_date, required=True)
    parser.add_argument("--end", type=parse_date, required=True,
                        help="Exclusive UTC end date")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--retries", type=int, default=2)
    args = parser.parse_args()
    if args.workers <= 0:
        parser.error("--workers must be positive")

    hours = download_range(
        args.symbol,
        args.start,
        args.end,
        workers=args.workers,
        timeout=args.timeout,
        retries=args.retries,
    )
    rows, sha256 = write_csv(args.output, hours)
    if rows == 0:
        raise RuntimeError("provider returned no ticks for the requested range")
    write_manifest(
        args.manifest,
        symbol=args.symbol,
        start=args.start,
        end=args.end,
        hours=hours,
        csv_path=args.output,
        rows=rows,
        sha256=sha256,
    )
    print(f"Wrote {rows:,} real {args.symbol.upper()} ticks to {args.output}")
    print(f"Manifest: {args.manifest}")
    print(f"SHA-256: {sha256}")


if __name__ == "__main__":
    main()
