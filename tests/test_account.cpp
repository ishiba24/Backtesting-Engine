#include "backtest/account.hpp"

#include <gtest/gtest.h>

using namespace backtest;

namespace {
InstrumentRegistry registry_with_eurusd() {
    InstrumentRegistry registry;
    registry.ensure_fx("EURUSD");
    return registry;
}
}  // namespace

TEST(Account, LongRoundTripProducesExactRealizedPnl) {
    auto registry = registry_with_eurusd();
    Account account(Money::from_integer(100'000));
    const auto& instrument = registry.get(0);
    account.apply_fill(0, Side::buy, Quantity::from_integer(10'000),
                       Price::parse("1.100200"), Money{}, instrument);
    const Money pnl = account.apply_fill(
        0, Side::sell, Quantity::from_integer(10'000),
        Price::parse("1.110000"), Money{}, instrument);
    EXPECT_EQ(pnl, Money::from_integer(98));
    EXPECT_EQ(account.balance(), Money::from_integer(100'098));
    EXPECT_EQ(account.position(0)->quantity, Quantity{});
}

TEST(Account, ShortRoundTripProducesExactRealizedPnl) {
    auto registry = registry_with_eurusd();
    Account account(Money::from_integer(100'000));
    const auto& instrument = registry.get(0);
    account.apply_fill(0, Side::sell, Quantity::from_integer(10'000),
                       Price::parse("1.100000"), Money{}, instrument);
    const Money pnl = account.apply_fill(
        0, Side::buy, Quantity::from_integer(10'000),
        Price::parse("1.090200"), Money{}, instrument);
    EXPECT_EQ(pnl, Money::from_integer(98));
    EXPECT_EQ(account.balance(), Money::from_integer(100'098));
}

TEST(Account, ReversalClosesOldSideAndUsesFillAsNewEntry) {
    auto registry = registry_with_eurusd();
    Account account(Money::from_integer(100'000));
    const auto& instrument = registry.get(0);
    account.apply_fill(0, Side::buy, Quantity::from_integer(10'000),
                       Price::parse("1.100000"), Money{}, instrument);
    account.apply_fill(0, Side::sell, Quantity::from_integer(15'000),
                       Price::parse("1.110000"), Money{}, instrument);
    ASSERT_NE(account.position(0), nullptr);
    EXPECT_EQ(account.position(0)->quantity, Quantity::from_integer(-5'000));
    EXPECT_EQ(account.position(0)->average_entry, Price::parse("1.110000"));
    EXPECT_EQ(account.position(0)->realized_pnl, Money::from_integer(100));
}
