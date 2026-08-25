#include "backtest/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace backtest {
namespace {

constexpr Timestamp kNanosecondsPerDay = 86'400'000'000'000LL;
constexpr long double kNanosecondsPerYear = 365.25L * kNanosecondsPerDay;
constexpr double kTradingDaysPerYear = 252.0;

std::vector<double> daily_returns(const std::vector<EquityPoint>& curve) {
    std::vector<Money> daily_closes;
    if (curve.empty()) {
        return {};
    }
    Timestamp current_day = curve.front().ts / kNanosecondsPerDay;
    for (const EquityPoint& point : curve) {
        const Timestamp day = point.ts / kNanosecondsPerDay;
        if (day != current_day) {
            current_day = day;
            daily_closes.push_back(point.equity);
        } else if (daily_closes.empty()) {
            daily_closes.push_back(point.equity);
        } else {
            daily_closes.back() = point.equity;
        }
    }

    std::vector<double> returns;
    returns.reserve(daily_closes.size() > 0 ? daily_closes.size() - 1 : 0);
    for (std::size_t i = 1; i < daily_closes.size(); ++i) {
        if (daily_closes[i - 1] > Money{}) {
            returns.push_back(
                daily_closes[i].to_double() / daily_closes[i - 1].to_double() - 1.0);
        }
    }
    return returns;
}

double sample_stddev(const std::vector<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
        static_cast<double>(values.size());
    double sum = 0.0;
    for (double value : values) {
        const double difference = value - mean;
        sum += difference * difference;
    }
    return std::sqrt(sum / static_cast<double>(values.size() - 1));
}

double drawdown(const std::vector<EquityPoint>& curve) {
    if (curve.empty()) {
        return 0.0;
    }
    Money peak = curve.front().equity;
    double worst = 0.0;
    for (const EquityPoint& point : curve) {
        if (point.equity > peak) {
            peak = point.equity;
        }
        if (peak > Money{}) {
            worst = std::max(
                worst, 1.0 - point.equity.to_double() / peak.to_double());
        }
    }
    return worst;
}

}  // namespace

Metrics compute_metrics(
    const RunResult& result, Money initial_balance,
    const std::string& strategy_name) {
    Metrics metrics;
    metrics.strategy_name = strategy_name;
    metrics.ticks = result.ticks_processed;
    metrics.trades = result.trades.size();
    metrics.rejected_orders = result.rejected_orders;
    metrics.initial_balance = initial_balance;
    metrics.final_balance = result.final_account.balance;
    metrics.final_equity = result.final_account.equity;
    metrics.realized_pnl = result.final_account.realized_pnl;
    metrics.unrealized_pnl = result.final_account.unrealized_pnl;
    metrics.total_fees = result.final_account.total_fees;

    int closed = 0;
    int wins = 0;
    for (const Trade& trade : result.trades) {
        metrics.long_fills += trade.side == Side::buy ? 1 : 0;
        metrics.short_fills += trade.side == Side::sell ? 1 : 0;
        metrics.stop_exits += trade.reason == FillReason::stop_loss ? 1 : 0;
        metrics.take_profit_exits += trade.reason == FillReason::take_profit ? 1 : 0;
        if (trade.closed_quantity > Quantity{}) {
            ++closed;
            wins += trade.realized_pnl > Money{} ? 1 : 0;
        }
    }
    metrics.win_rate = closed > 0 ? static_cast<double>(wins) / closed : 0.0;

    if (initial_balance > Money{}) {
        metrics.total_return =
            metrics.final_equity.to_double() / initial_balance.to_double() - 1.0;
    }
    metrics.max_drawdown = drawdown(result.equity_curve);

    if (result.equity_curve.size() >= 2 && initial_balance > Money{} &&
        metrics.final_equity > Money{}) {
        const Timestamp elapsed =
            result.equity_curve.back().ts - result.equity_curve.front().ts;
        const long double years = static_cast<long double>(elapsed) /
            kNanosecondsPerYear;
        if (years >= (1.0L / 365.25L)) {
            const double annualized = std::pow(
                metrics.final_equity.to_double() / initial_balance.to_double(),
                1.0 / static_cast<double>(years)) - 1.0;
            if (std::isfinite(annualized)) {
                metrics.cagr = annualized;
            }
        }
    }

    const auto returns = daily_returns(result.equity_curve);
    if (!returns.empty()) {
        const double mean = std::accumulate(returns.begin(), returns.end(), 0.0) /
            static_cast<double>(returns.size());
        const double standard_deviation = sample_stddev(returns);
        metrics.volatility = standard_deviation * std::sqrt(kTradingDaysPerYear);
        if (standard_deviation > 0.0) {
            metrics.sharpe = mean / standard_deviation *
                std::sqrt(kTradingDaysPerYear);
        }
    }
    return metrics;
}

void print_metrics(const Metrics& metrics, const InstrumentRegistry&) {
    std::cout << std::fixed << std::setprecision(4)
              << "\n========== FX Tick Backtest ==========" << '\n'
              << "Strategy:          " << metrics.strategy_name << '\n'
              << "Ticks:             " << metrics.ticks << '\n'
              << "Trades:            " << metrics.trades << '\n'
              << "Rejected orders:   " << metrics.rejected_orders << '\n'
              << "Initial balance:   " << metrics.initial_balance.to_double() << '\n'
              << "Final balance:     " << metrics.final_balance.to_double() << '\n'
              << "Final equity:      " << metrics.final_equity.to_double() << '\n'
              << "Realized PnL:      " << metrics.realized_pnl.to_double() << '\n'
              << "Unrealized PnL:    " << metrics.unrealized_pnl.to_double() << '\n'
              << "Fees:              " << metrics.total_fees.to_double() << '\n'
              << "Total return:      " << metrics.total_return * 100.0 << " %\n"
              << "CAGR:              " << metrics.cagr * 100.0 << " %\n"
              << "Max drawdown:      " << metrics.max_drawdown * 100.0 << " %\n"
              << "Sharpe (daily):    " << metrics.sharpe << '\n'
              << "Volatility (ann.): " << metrics.volatility * 100.0 << " %\n"
              << "Win rate:          " << metrics.win_rate * 100.0 << " %\n"
              << "Stop exits:        " << metrics.stop_exits << '\n'
              << "Take-profit exits: " << metrics.take_profit_exits << '\n'
              << "======================================" << '\n';
}

bool write_equity_csv(const std::string& path, const std::vector<EquityPoint>& curve) {
    std::ofstream out(path);
    if (!out) return false;
    out << "timestamp_ns,balance,equity,realized_pnl,unrealized_pnl,used_margin\n";
    for (const EquityPoint& point : curve) {
        out << point.ts << ',' << point.balance.to_string() << ','
            << point.equity.to_string() << ',' << point.realized_pnl.to_string()
            << ',' << point.unrealized_pnl.to_string() << ','
            << point.used_margin.to_string() << '\n';
    }
    return static_cast<bool>(out);
}

bool write_trades_csv(
    const std::string& path, const std::vector<Trade>& trades,
    const InstrumentRegistry& registry) {
    std::ofstream out(path);
    if (!out) return false;
    out << "timestamp_ns,symbol,side,quantity,price,fee,realized_pnl,closed_quantity,reason\n";
    for (const Trade& trade : trades) {
        out << trade.ts << ',' << registry.get(trade.instrument_id).symbol << ','
            << to_string(trade.side) << ',' << trade.quantity.to_string() << ','
            << trade.price.to_string() << ',' << trade.fee.to_string() << ','
            << trade.realized_pnl.to_string() << ','
            << trade.closed_quantity.to_string() << ','
            << to_string(trade.reason) << '\n';
    }
    return static_cast<bool>(out);
}

bool write_summary_json(const std::string& path, const Metrics& metrics) {
    std::ofstream out(path);
    if (!out) return false;
    out << std::setprecision(10)
        << "{\n"
        << "  \"strategy\": \"" << metrics.strategy_name << "\",\n"
        << "  \"ticks\": " << metrics.ticks << ",\n"
        << "  \"trades\": " << metrics.trades << ",\n"
        << "  \"final_balance\": \"" << metrics.final_balance.to_string() << "\",\n"
        << "  \"final_equity\": \"" << metrics.final_equity.to_string() << "\",\n"
        << "  \"realized_pnl\": \"" << metrics.realized_pnl.to_string() << "\",\n"
        << "  \"unrealized_pnl\": \"" << metrics.unrealized_pnl.to_string() << "\",\n"
        << "  \"total_fees\": \"" << metrics.total_fees.to_string() << "\",\n"
        << "  \"total_return\": " << metrics.total_return << ",\n"
        << "  \"cagr\": " << metrics.cagr << ",\n"
        << "  \"max_drawdown\": " << metrics.max_drawdown << ",\n"
        << "  \"sharpe\": " << metrics.sharpe << ",\n"
        << "  \"volatility\": " << metrics.volatility << ",\n"
        << "  \"win_rate\": " << metrics.win_rate << ",\n"
        << "  \"stop_exits\": " << metrics.stop_exits << ",\n"
        << "  \"take_profit_exits\": " << metrics.take_profit_exits << "\n"
        << "}\n";
    return static_cast<bool>(out);
}

}  // namespace backtest
