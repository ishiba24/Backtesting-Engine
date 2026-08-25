#include "backtest/account.hpp"

#include <stdexcept>

namespace backtest {
namespace {

Price liquidation_price(const Position& position, const Tick& quote) {
    return position.quantity > Quantity{} ? quote.bid : quote.ask;
}

}  // namespace

Account::Account(Money starting_balance) : balance_(starting_balance) {
    if (starting_balance < Money{}) {
        throw std::invalid_argument("starting balance cannot be negative");
    }
}

const Position* Account::position(InstrumentId id) const {
    const auto it = positions_.find(id);
    return it == positions_.end() ? nullptr : &it->second;
}

Position* Account::position(InstrumentId id) {
    const auto it = positions_.find(id);
    return it == positions_.end() ? nullptr : &it->second;
}

Money Account::total_realized_pnl() const {
    Money total;
    for (const auto& [id, position] : positions_) {
        (void)id;
        total += position.realized_pnl;
    }
    return total;
}

Money position_notional(
    Quantity quantity, Price price, const Instrument& instrument) {
    return abs(quantity) * price * instrument.contract_size *
        instrument.quote_to_account_rate;
}

Money Account::apply_fill(
    InstrumentId id, Side side, Quantity quantity, Price price,
    Money fee, const Instrument& instrument) {
    if (quantity <= Quantity{} || price <= Price{} || fee < Money{}) {
        throw std::invalid_argument("invalid fill");
    }
    Position& pos = positions_[id];
    pos.instrument_id = id;
    const Quantity delta = side == Side::buy ? quantity : -quantity;
    const Quantity old_quantity = pos.quantity;
    Money realized;

    if (old_quantity == Quantity{} ||
        (old_quantity > Quantity{}) == (delta > Quantity{})) {
        const Quantity combined = abs(old_quantity) + abs(delta);
        if (combined > Quantity{}) {
            pos.average_entry =
                (pos.average_entry * abs(old_quantity) + price * abs(delta)) /
                combined;
        }
        pos.quantity += delta;
    } else {
        const Quantity closing =
            abs(delta) < abs(old_quantity) ? abs(delta) : abs(old_quantity);
        const Decimal direction = old_quantity > Quantity{}
            ? Decimal::from_integer(1)
            : Decimal::from_integer(-1);
        realized = closing * (price - pos.average_entry) *
            instrument.contract_size * instrument.quote_to_account_rate * direction;
        pos.realized_pnl += realized;
        pos.quantity += delta;

        if (pos.quantity == Quantity{}) {
            pos.average_entry = Price{};
        } else if ((pos.quantity > Quantity{}) != (old_quantity > Quantity{})) {
            // The fill crossed through zero and opened a position the other way.
            pos.average_entry = price;
        }
    }

    balance_ += realized - fee;
    total_fees_ += fee;
    pos.fees += fee;
    return realized;
}

Money Account::unrealized_pnl(
    const InstrumentRegistry& registry,
    std::span<const std::optional<Tick>> quotes) const {
    Money total;
    for (const auto& [id, pos] : positions_) {
        if (pos.quantity == Quantity{} || id >= quotes.size() || !quotes[id]) {
            continue;
        }
        const Instrument& instrument = registry.get(id);
        const Price mark = liquidation_price(pos, *quotes[id]);
        const Decimal direction = pos.quantity > Quantity{}
            ? Decimal::from_integer(1)
            : Decimal::from_integer(-1);
        total += abs(pos.quantity) * (mark - pos.average_entry) *
            instrument.contract_size * instrument.quote_to_account_rate * direction;
    }
    return total;
}

Money Account::used_margin(
    const AccountConfig& config, const InstrumentRegistry& registry,
    std::span<const std::optional<Tick>> quotes) const {
    if (config.leverage <= Decimal{}) {
        throw std::invalid_argument("leverage must be positive");
    }
    Money total;
    for (const auto& [id, pos] : positions_) {
        if (pos.quantity == Quantity{} || id >= quotes.size() || !quotes[id]) {
            continue;
        }
        const Tick& quote = *quotes[id];
        const Price mid = (quote.bid + quote.ask) / Decimal::from_integer(2);
        total += position_notional(pos.quantity, mid, registry.get(id)) /
            config.leverage;
    }
    return total;
}

AccountSnapshot Account::snapshot(
    const AccountConfig& config, const InstrumentRegistry& registry,
    std::span<const std::optional<Tick>> quotes) const {
    AccountSnapshot result;
    result.balance = balance_;
    result.realized_pnl = total_realized_pnl();
    result.unrealized_pnl = unrealized_pnl(registry, quotes);
    result.equity = balance_ + result.unrealized_pnl;
    result.used_margin = used_margin(config, registry, quotes);
    result.total_fees = total_fees_;
    return result;
}

}  // namespace backtest
