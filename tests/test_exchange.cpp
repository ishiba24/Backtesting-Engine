#include "backtest/exchange.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using namespace backtest;

namespace {
Tick quote(InstrumentId id) {
    return Tick{.ts = 7, .instrument_id = id,
                .bid = Price::parse("1.100000"),
                .ask = Price::parse("1.100200"),
                .bid_size = Quantity::from_integer(1'000'000),
                .ask_size = Quantity::from_integer(1'000'000)};
}
}  // namespace

TEST(Exchange, BuysAtAskAndSellsAtBid) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    AccountConfig config;
    Account account(config.starting_balance);
    std::vector<std::optional<Tick>> quotes{quote(id)};
    Exchange exchange;

    const auto buy = exchange.execute_market_order(
        account, config, registry, *quotes[0],
        Order{.instrument_id = id, .side = Side::buy,
              .quantity = Quantity::from_integer(10'000)},
        account.snapshot(config, registry, quotes));
    ASSERT_TRUE(buy);
    EXPECT_EQ(buy->price, Price::parse("1.100200"));

    const auto sell = exchange.execute_market_order(
        account, config, registry, *quotes[0],
        Order{.instrument_id = id, .side = Side::sell,
              .quantity = Quantity::from_integer(10'000)},
        account.snapshot(config, registry, quotes));
    ASSERT_TRUE(sell);
    EXPECT_EQ(sell->price, Price::parse("1.100000"));
    EXPECT_EQ(sell->realized_pnl, Money::from_integer(-2));
}

TEST(Exchange, RejectsPositionWhoseMarginExceedsEquity) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    AccountConfig config;
    config.starting_balance = Money::from_integer(1'000);
    config.leverage = Decimal::from_integer(10);
    Account account(config.starting_balance);
    std::vector<std::optional<Tick>> quotes{quote(id)};
    Exchange exchange;
    const auto fill = exchange.execute_market_order(
        account, config, registry, *quotes[0],
        Order{.instrument_id = id, .side = Side::buy,
              .quantity = Quantity::from_integer(10'000)},
        account.snapshot(config, registry, quotes));
    EXPECT_FALSE(fill);
    EXPECT_EQ(account.position(id), nullptr);
}

TEST(Exchange, AppliesConfiguredSlippageAndFixedPointFee) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    AccountConfig config;
    config.slippage_bps = 10;
    config.fee_bps = 1;
    Account account(config.starting_balance);
    std::vector<std::optional<Tick>> quotes{quote(id)};
    Exchange exchange;
    const auto fill = exchange.execute_market_order(
        account, config, registry, *quotes[0],
        Order{.instrument_id = id, .side = Side::buy,
              .quantity = Quantity::from_integer(10'000)},
        account.snapshot(config, registry, quotes));
    ASSERT_TRUE(fill);
    EXPECT_EQ(fill->price, Price::parse("1.101300"));
    EXPECT_EQ(fill->fee, Money::parse("1.101300"));
    EXPECT_EQ(account.balance(), Money::parse("99998.898700"));
}
