#pragma once

#include "backtest/instrument.hpp"
#include "backtest/tick.hpp"
#include "backtest/types.hpp"

#include <optional>
#include <span>
#include <unordered_map>

namespace backtest {

struct RiskConfig {
    std::uint32_t stop_loss_bps = 0;
    std::uint32_t take_profit_bps = 0;
};

struct AccountConfig {
    Money starting_balance = Money::from_integer(100'000);
    Decimal leverage = Decimal::from_integer(30);
    std::uint32_t fee_bps = 0;
    Money min_fee;
    std::uint32_t slippage_bps = 0;
    RiskConfig risk;
};

struct Position {
    InstrumentId instrument_id = 0;
    Quantity quantity;  // positive is long; negative is short.
    Price average_entry;
    Money realized_pnl;
    Money fees;
};

struct AccountSnapshot {
    Money balance;
    Money realized_pnl;
    Money unrealized_pnl;
    Money equity;
    Money used_margin;
    Money total_fees;
};

class Account {
public:
    explicit Account(Money starting_balance = Money{});

    Money balance() const { return balance_; }
    Money total_fees() const { return total_fees_; }
    Money total_realized_pnl() const;

    const Position* position(InstrumentId id) const;
    Position* position(InstrumentId id);
    const std::unordered_map<InstrumentId, Position>& positions() const {
        return positions_;
    }

    // Applies a completed fill and returns realized PnL before the fill fee.
    Money apply_fill(
        InstrumentId id, Side side, Quantity quantity, Price price,
        Money fee, const Instrument& instrument);

    Money unrealized_pnl(
        const InstrumentRegistry& registry,
        std::span<const std::optional<Tick>> quotes) const;
    Money used_margin(
        const AccountConfig& config, const InstrumentRegistry& registry,
        std::span<const std::optional<Tick>> quotes) const;
    AccountSnapshot snapshot(
        const AccountConfig& config, const InstrumentRegistry& registry,
        std::span<const std::optional<Tick>> quotes) const;

private:
    Money balance_;
    Money total_fees_;
    std::unordered_map<InstrumentId, Position> positions_;
};

Money position_notional(
    Quantity quantity, Price price, const Instrument& instrument);

}  // namespace backtest
