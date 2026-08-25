#pragma once

#include "backtest/types.hpp"

namespace backtest {

struct Order {
    InstrumentId instrument_id = 0;
    Side side = Side::buy;
    OrderType type = OrderType::market;
    Quantity quantity;
    FillReason reason = FillReason::strategy;
};

}  // namespace backtest
