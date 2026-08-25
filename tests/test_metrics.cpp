#include "backtest/metrics.hpp"

#include <gtest/gtest.h>

using namespace backtest;

TEST(Metrics, ReportsPnlRiskExitsAndDrawdown) {
    RunResult result;
    result.ticks_processed = 3;
    result.final_account.balance = Money::from_integer(99'900);
    result.final_account.equity = Money::from_integer(99'950);
    result.final_account.realized_pnl = Money::from_integer(-100);
    result.final_account.unrealized_pnl = Money::from_integer(50);
    result.final_account.total_fees = Money::from_integer(2);
    result.equity_curve = {
        EquityPoint{.ts = 1, .equity = Money::from_integer(100'000)},
        EquityPoint{.ts = 2, .equity = Money::from_integer(101'000)},
        EquityPoint{.ts = 3, .equity = Money::from_integer(99'950)}};
    result.trades = {
        Trade{.side = Side::buy, .quantity = Quantity::from_integer(1),
              .fee = Money::parse("1.000000")},
        Trade{.side = Side::sell, .quantity = Quantity::from_integer(1),
              .fee = Money::parse("1.000000"),
              .realized_pnl = Money::from_integer(-100),
              .closed_quantity = Quantity::from_integer(1),
              .reason = FillReason::stop_loss}};

    const Metrics metrics = compute_metrics(
        result, Money::from_integer(100'000), "test");
    EXPECT_EQ(metrics.stop_exits, 1u);
    EXPECT_EQ(metrics.take_profit_exits, 0u);
    EXPECT_EQ(metrics.total_fees, Money::from_integer(2));
    EXPECT_NEAR(metrics.total_return, -0.0005, 1e-12);
    EXPECT_NEAR(metrics.max_drawdown, 1.0 - 99'950.0 / 101'000.0, 1e-12);
    EXPECT_DOUBLE_EQ(metrics.win_rate, 0.0);
    EXPECT_DOUBLE_EQ(metrics.cagr, 0.0);  // Do not annualize nanosecond fixtures.
}
