#include "backtest/engine.hpp"

#include "backtest/exchange.hpp"

#include <optional>
#include <stdexcept>

namespace backtest {
namespace {

std::optional<Order> risk_exit(
    const Position& position, const Tick& tick, const RiskConfig& risk) {
    if (position.quantity == Quantity{}) {
        return std::nullopt;
    }

    const bool is_long = position.quantity > Quantity{};
    const Price executable = is_long ? tick.bid : tick.ask;
    if (risk.stop_loss_bps > 0) {
        const std::int32_t adjustment = is_long
            ? -static_cast<std::int32_t>(risk.stop_loss_bps)
            : static_cast<std::int32_t>(risk.stop_loss_bps);
        const Price stop = apply_bps(position.average_entry, adjustment);
        if ((is_long && executable <= stop) || (!is_long && executable >= stop)) {
            return Order{
                .instrument_id = position.instrument_id,
                .side = is_long ? Side::sell : Side::buy,
                .quantity = abs(position.quantity),
                .reason = FillReason::stop_loss,
            };
        }
    }

    if (risk.take_profit_bps > 0) {
        const std::int32_t adjustment = is_long
            ? static_cast<std::int32_t>(risk.take_profit_bps)
            : -static_cast<std::int32_t>(risk.take_profit_bps);
        const Price target = apply_bps(position.average_entry, adjustment);
        if ((is_long && executable >= target) || (!is_long && executable <= target)) {
            return Order{
                .instrument_id = position.instrument_id,
                .side = is_long ? Side::sell : Side::buy,
                .quantity = abs(position.quantity),
                .reason = FillReason::take_profit,
            };
        }
    }
    return std::nullopt;
}

EquityPoint make_equity_point(Timestamp ts, const AccountSnapshot& snapshot) {
    return EquityPoint{
        .ts = ts,
        .balance = snapshot.balance,
        .equity = snapshot.equity,
        .realized_pnl = snapshot.realized_pnl,
        .unrealized_pnl = snapshot.unrealized_pnl,
        .used_margin = snapshot.used_margin,
    };
}

}  // namespace

RunResult run(
    IStrategy& strategy, std::span<const Tick> ticks,
    const InstrumentRegistry& registry, const EngineConfig& config) {
    RunResult result;
    if (config.account.leverage <= Decimal{}) {
        throw std::invalid_argument("leverage must be positive");
    }
    if (config.account.fee_bps > 10'000 ||
        config.account.slippage_bps > 10'000 ||
        config.account.risk.stop_loss_bps >= 10'000 ||
        config.account.risk.take_profit_bps > 100'000 ||
        config.account.min_fee < Money{}) {
        throw std::invalid_argument("invalid basis-point configuration");
    }
    if (ticks.empty()) {
        return result;
    }

    Account account(config.account.starting_balance);
    Exchange exchange;
    std::vector<std::optional<Tick>> latest_quotes(registry.size());
    std::vector<std::vector<Order>> pending(registry.size());
    std::vector<Order> emitted;
    emitted.reserve(4);
    if (config.log_equity_curve) {
        result.equity_curve.reserve(ticks.size());
    }
    if (config.log_trades) {
        result.trades.reserve(ticks.size() / 100 + 8);
    }

    for (const Tick& tick : ticks) {
        if (tick.instrument_id >= registry.size()) {
            ++result.rejected_orders;
            continue;
        }
        latest_quotes[tick.instrument_id] = tick;

        // Strategy orders wait for the next quote for this instrument, which
        // preserves the no-look-ahead decision/execution boundary.
        auto& queued = pending[tick.instrument_id];
        for (const Order& order : queued) {
            const AccountSnapshot before =
                account.snapshot(config.account, registry, latest_quotes);
            auto trade = exchange.execute_market_order(
                account, config.account, registry, tick, order, before);
            if (trade) {
                if (config.log_trades) {
                    result.trades.push_back(*trade);
                }
            } else {
                ++result.rejected_orders;
            }
        }
        queued.clear();

        // Protective exits trigger against the executable side of the quote:
        // bid for a long liquidation, ask for a short cover.
        if (const Position* position = account.position(tick.instrument_id)) {
            if (auto exit = risk_exit(*position, tick, config.account.risk)) {
                const AccountSnapshot before =
                    account.snapshot(config.account, registry, latest_quotes);
                auto trade = exchange.execute_market_order(
                    account, config.account, registry, tick, *exit, before);
                if (trade) {
                    if (config.log_trades) {
                        result.trades.push_back(*trade);
                    }
                } else {
                    ++result.rejected_orders;
                }
            }
        }

        const AccountSnapshot snapshot =
            account.snapshot(config.account, registry, latest_quotes);
        if (config.log_equity_curve) {
            result.equity_curve.push_back(make_equity_point(tick.ts, snapshot));
        }
        result.final_account = snapshot;
        ++result.ticks_processed;

        const Position* position = account.position(tick.instrument_id);
        emitted.clear();
        strategy.on_tick(
            StrategyContext{
                .tick = tick,
                .instrument = registry.get(tick.instrument_id),
                .equity = snapshot.equity,
                .position_quantity = position ? position->quantity : Quantity{},
            },
            emitted);
        for (const Order& order : emitted) {
            if (order.instrument_id >= pending.size()) {
                ++result.rejected_orders;
            } else {
                pending[order.instrument_id].push_back(order);
            }
        }
    }

    result.final_positions.reserve(account.positions().size());
    for (const auto& [id, position] : account.positions()) {
        (void)id;
        result.final_positions.push_back(position);
    }
    return result;
}

}  // namespace backtest
