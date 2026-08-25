#include "backtest/strategies/sma_crossover.hpp"

#include <gtest/gtest.h>

using namespace backtest;

TEST(Strategy, SmaCrossoverMovesFromLongTargetToShortTarget) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    SmaCrossover strategy(id, 2, 3, Quantity::from_integer(10));
    const std::vector<double> mids{3, 3, 3, 1, 5, 6, 7, 2, 1};
    Quantity position;
    std::vector<Order> all;
    for (std::size_t i = 0; i < mids.size(); ++i) {
        const Price mid = Price::from_double(mids[i]);
        const Tick tick{.ts = static_cast<Timestamp>(i), .instrument_id = id,
                        .bid = mid, .ask = mid};
        std::vector<Order> orders;
        strategy.on_tick(
            StrategyContext{.tick = tick, .instrument = registry.get(id),
                            .equity = Money::from_integer(1000),
                            .position_quantity = position},
            orders);
        for (const Order& order : orders) {
            all.push_back(order);
            position += order.side == Side::buy ? order.quantity : -order.quantity;
        }
    }
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].side, Side::buy);
    EXPECT_EQ(all[0].quantity, Quantity::from_integer(10));
    EXPECT_EQ(all[1].side, Side::sell);
    EXPECT_EQ(all[1].quantity, Quantity::from_integer(20));
}

TEST(Strategy, RejectsInvalidSmaPeriods) {
    EXPECT_THROW(SmaCrossover(0, 5, 5, Quantity::from_integer(1)),
                 std::invalid_argument);
    EXPECT_THROW(SmaCrossover(0, 0, 5, Quantity::from_integer(1)),
                 std::invalid_argument);
}
