#pragma once

#include "backtest/types.hpp"

namespace backtest {

struct Trade {
    Timestamp ts = 0;
    InstrumentId instrument_id = 0;
    Side side = Side::buy;
    Quantity quantity;
    Price price;
    Money fee;
    Money realized_pnl;
    Quantity closed_quantity;
    FillReason reason = FillReason::strategy;
};

}  // namespace backtest
