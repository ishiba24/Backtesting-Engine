#pragma once

#include "backtest/account.hpp"
#include "backtest/order.hpp"
#include "backtest/trade.hpp"

#include <optional>

namespace backtest {

class Exchange {
public:
    std::optional<Trade> execute_market_order(
        Account& account, const AccountConfig& config,
        const InstrumentRegistry& registry, const Tick& quote,
        const Order& order, const AccountSnapshot& before) const;
};

}  // namespace backtest
