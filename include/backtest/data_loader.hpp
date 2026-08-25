#pragma once

#include "backtest/instrument.hpp"
#include "backtest/tick.hpp"

#include <span>
#include <string>

namespace backtest {

// CSV schema: timestamp_ns,symbol,bid,ask,bid_size,ask_size
TickSeries load_ticks_csv(const std::string& path, InstrumentRegistry& registry);

std::span<const Tick> slice_by_time(
    std::span<const Tick> ticks, Timestamp start, Timestamp end);

}  // namespace backtest
