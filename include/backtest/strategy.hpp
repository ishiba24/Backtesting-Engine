#pragma once

#include "backtest/instrument.hpp"
#include "backtest/order.hpp"
#include "backtest/tick.hpp"

#include <string>
#include <vector>

namespace backtest {

struct StrategyContext {
    const Tick& tick;
    const Instrument& instrument;
    Money equity;
    Quantity position_quantity;
};

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual void on_tick(
        const StrategyContext& context, std::vector<Order>& orders) = 0;
    virtual std::string name() const = 0;
};

}  // namespace backtest
