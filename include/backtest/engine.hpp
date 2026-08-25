#pragma once

#include "backtest/account.hpp"
#include "backtest/strategy.hpp"
#include "backtest/trade.hpp"

#include <span>
#include <vector>

namespace backtest {

struct EquityPoint {
    Timestamp ts = 0;
    Money balance;
    Money equity;
    Money realized_pnl;
    Money unrealized_pnl;
    Money used_margin;
};

struct EngineConfig {
    AccountConfig account;
    bool log_trades = true;
    bool log_equity_curve = true;
};

struct RunResult {
    std::vector<EquityPoint> equity_curve;
    std::vector<Trade> trades;
    std::vector<Position> final_positions;
    AccountSnapshot final_account;
    std::size_t ticks_processed = 0;
    std::size_t rejected_orders = 0;
};

// std::span is a C++20 non-owning view: callers can replay vectors, arrays, or
// slices without copying tick data or templating the public API.
RunResult run(
    IStrategy& strategy, std::span<const Tick> ticks,
    const InstrumentRegistry& registry, const EngineConfig& config);

}  // namespace backtest
