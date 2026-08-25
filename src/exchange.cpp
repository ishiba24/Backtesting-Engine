#include "backtest/exchange.hpp"

#include <algorithm>

namespace backtest {

std::optional<Trade> Exchange::execute_market_order(
    Account& account, const AccountConfig& config,
    const InstrumentRegistry& registry, const Tick& quote,
    const Order& order, const AccountSnapshot& before) const {
    if (order.type != OrderType::market ||
        order.instrument_id != quote.instrument_id ||
        order.quantity <= Quantity{}) {
        return std::nullopt;
    }

    const Instrument& instrument = registry.get(order.instrument_id);
    const Price raw_price = order.side == Side::buy ? quote.ask : quote.bid;
    const std::int32_t signed_slippage = order.side == Side::buy
        ? static_cast<std::int32_t>(config.slippage_bps)
        : -static_cast<std::int32_t>(config.slippage_bps);
    const Price fill_price = apply_bps(raw_price, signed_slippage);
    const Money notional = position_notional(order.quantity, fill_price, instrument);
    const Money proportional_fee = apply_bps(notional, config.fee_bps) - notional;
    const Money fee = proportional_fee < config.min_fee
        ? config.min_fee
        : proportional_fee;

    const Position* current = account.position(order.instrument_id);
    const Quantity old_quantity = current ? current->quantity : Quantity{};
    const Quantity delta = order.side == Side::buy ? order.quantity : -order.quantity;
    const Quantity new_quantity = old_quantity + delta;
    const Price margin_price = (quote.bid + quote.ask) / Decimal::from_integer(2);
    const Money old_margin = position_notional(old_quantity, margin_price, instrument) /
        config.leverage;
    const Money new_margin = position_notional(new_quantity, margin_price, instrument) /
        config.leverage;
    const Money projected_margin = before.used_margin - old_margin + new_margin;

    // Closing risk is always permitted; increases must fit available equity.
    if (abs(new_quantity) > abs(old_quantity) && projected_margin + fee > before.equity) {
        return std::nullopt;
    }

    Trade trade;
    trade.ts = quote.ts;
    trade.instrument_id = order.instrument_id;
    trade.side = order.side;
    trade.quantity = order.quantity;
    trade.price = fill_price;
    trade.fee = fee;
    trade.reason = order.reason;
    if (old_quantity != Quantity{} &&
        (old_quantity > Quantity{}) != (delta > Quantity{})) {
        trade.closed_quantity =
            abs(delta) < abs(old_quantity) ? abs(delta) : abs(old_quantity);
    }
    trade.realized_pnl = account.apply_fill(
        order.instrument_id, order.side, order.quantity, fill_price, fee, instrument);
    return trade;
}

}  // namespace backtest
