#pragma once

#include "backtest/types.hpp"

#include <vector>

namespace backtest {

struct Tick {
    Timestamp ts = 0;
    InstrumentId instrument_id = 0;
    Price bid;
    Price ask;
    Quantity bid_size;
    Quantity ask_size;
};

using TickSeries = std::vector<Tick>;

}  // namespace backtest
