#pragma once

#include "backtest/strategy.hpp"

#include <stdexcept>

namespace backtest {

// Opens one long or short FX position and then leaves risk management to the
// engine. The name is retained as a simple baseline strategy.
class BuyAndHold : public IStrategy {
public:
    BuyAndHold(InstrumentId instrument_id, Quantity quantity, Side side = Side::buy)
        : instrument_id_(instrument_id), quantity_(quantity), side_(side) {
        if (quantity <= Quantity{}) {
            throw std::invalid_argument("target quantity must be positive");
        }
    }

    void on_tick(
        const StrategyContext& context, std::vector<Order>& orders) override {
        if (placed_ || context.tick.instrument_id != instrument_id_) {
            return;
        }
        orders.push_back(Order{
            .instrument_id = instrument_id_,
            .side = side_,
            .type = OrderType::market,
            .quantity = quantity_,
            .reason = FillReason::strategy,
        });
        placed_ = true;
    }

    std::string name() const override { return "buy_and_hold"; }

private:
    InstrumentId instrument_id_;
    Quantity quantity_;
    Side side_;
    bool placed_ = false;
};

}  // namespace backtest
