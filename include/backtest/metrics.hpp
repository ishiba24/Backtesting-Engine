#pragma once

#include "backtest/engine.hpp"

#include <string>

namespace backtest {

struct Metrics {
    std::string strategy_name;
    std::size_t ticks = 0;
    std::size_t trades = 0;
    std::size_t long_fills = 0;
    std::size_t short_fills = 0;
    std::size_t stop_exits = 0;
    std::size_t take_profit_exits = 0;
    std::size_t rejected_orders = 0;
    Money initial_balance;
    Money final_balance;
    Money final_equity;
    Money realized_pnl;
    Money unrealized_pnl;
    Money total_fees;
    double total_return = 0.0;
    double cagr = 0.0;
    double max_drawdown = 0.0;
    double sharpe = 0.0;
    double volatility = 0.0;
    double win_rate = 0.0;
};

Metrics compute_metrics(
    const RunResult& result, Money initial_balance,
    const std::string& strategy_name);
void print_metrics(const Metrics& metrics, const InstrumentRegistry& registry);
bool write_equity_csv(const std::string& path, const std::vector<EquityPoint>& curve);
bool write_trades_csv(
    const std::string& path, const std::vector<Trade>& trades,
    const InstrumentRegistry& registry);
bool write_summary_json(const std::string& path, const Metrics& metrics);

}  // namespace backtest
