#pragma once

#include "backtest/rolling_sma.hpp"
#include "backtest/strategy.hpp"

#include <cstddef>
#include <stdexcept>

namespace backtest {

class SmaCrossover : public IStrategy {
public:
    SmaCrossover(
        InstrumentId instrument_id, std::size_t fast_period,
        std::size_t slow_period, Quantity target_quantity,
        bool allow_short = true)
        : instrument_id_(instrument_id),
          fast_(fast_period),
          slow_(slow_period),
          target_quantity_(target_quantity),
          allow_short_(allow_short) {
        if (fast_period >= slow_period) {
            throw std::invalid_argument("fast SMA period must be less than slow period");
        }
        if (target_quantity <= Quantity{}) {
            throw std::invalid_argument("target quantity must be positive");
        }
    }

    void on_tick(
        const StrategyContext& context, std::vector<Order>& orders) override {
        if (context.tick.instrument_id != instrument_id_) {
            return;
        }
        const Price mid = (context.tick.bid + context.tick.ask) /
            Decimal::from_integer(2);
        fast_.push(mid);
        slow_.push(mid);
        if (!slow_.ready()) {
            return;
        }

        const bool fast_above = fast_.value() > slow_.value();
        if (have_previous_ && fast_above != previous_fast_above_) {
            if (fast_above) {
                const Quantity buy_quantity = target_quantity_ - context.position_quantity;
                if (buy_quantity > Quantity{}) {
                    orders.push_back(Order{
                        .instrument_id = instrument_id_,
                        .side = Side::buy,
                        .quantity = buy_quantity,
                    });
                }
            } else {
                const Quantity target = allow_short_ ? -target_quantity_ : Quantity{};
                const Quantity sell_quantity = context.position_quantity - target;
                if (sell_quantity > Quantity{}) {
                    orders.push_back(Order{
                        .instrument_id = instrument_id_,
                        .side = Side::sell,
                        .quantity = sell_quantity,
                    });
                }
            }
        }
        previous_fast_above_ = fast_above;
        have_previous_ = true;
    }

    std::string name() const override { return "sma_crossover"; }

private:
    InstrumentId instrument_id_;
    RollingSma fast_;
    RollingSma slow_;
    Quantity target_quantity_;
    bool allow_short_;
    bool previous_fast_above_ = false;
    bool have_previous_ = false;
};

}  // namespace backtest
