#include "backtest/engine.hpp"
#include "backtest/strategies/buy_and_hold.hpp"

#include <gtest/gtest.h>

using namespace backtest;

namespace {
Tick tick(Timestamp ts, InstrumentId id, const char* bid, const char* ask) {
    return Tick{.ts = ts, .instrument_id = id,
                .bid = Price::parse(bid), .ask = Price::parse(ask),
                .bid_size = Quantity::from_integer(1'000'000),
                .ask_size = Quantity::from_integer(1'000'000)};
}
}  // namespace

TEST(Engine, SignalExecutesOnNextQuoteAsk) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    const TickSeries ticks{
        tick(1, id, "1.000000", "1.000200"),
        tick(2, id, "1.100000", "1.100200"),
        tick(3, id, "1.110000", "1.110200")};
    BuyAndHold strategy(id, Quantity::from_integer(10'000));
    const auto result = run(strategy, ticks, registry, EngineConfig{});
    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].ts, 2);
    EXPECT_EQ(result.trades[0].price, Price::parse("1.100200"));
    EXPECT_EQ(result.final_account.unrealized_pnl, Money::from_integer(98));
}

TEST(Engine, LongStopLossClosesAtBidAndRecordsLoss) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    const TickSeries ticks{
        tick(1, id, "1.099800", "1.100000"),
        tick(2, id, "1.099800", "1.100000"),
        tick(3, id, "1.088000", "1.088200")};
    EngineConfig config;
    config.account.risk.stop_loss_bps = 100;
    BuyAndHold strategy(id, Quantity::from_integer(10'000));
    const auto result = run(strategy, ticks, registry, config);
    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[1].reason, FillReason::stop_loss);
    EXPECT_EQ(result.trades[1].side, Side::sell);
    EXPECT_EQ(result.trades[1].price, Price::parse("1.088000"));
    EXPECT_EQ(result.trades[1].realized_pnl, Money::from_integer(-120));
    EXPECT_EQ(result.final_account.equity, Money::from_integer(99'880));
}

TEST(Engine, ShortTakeProfitCoversAtAskAndRecordsProfit) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    const TickSeries ticks{
        tick(1, id, "1.100000", "1.100200"),
        tick(2, id, "1.100000", "1.100200"),
        tick(3, id, "1.087800", "1.088000")};
    EngineConfig config;
    config.account.risk.take_profit_bps = 100;
    BuyAndHold strategy(id, Quantity::from_integer(10'000), Side::sell);
    const auto result = run(strategy, ticks, registry, config);
    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[1].reason, FillReason::take_profit);
    EXPECT_EQ(result.trades[1].side, Side::buy);
    EXPECT_EQ(result.trades[1].realized_pnl, Money::from_integer(120));
    EXPECT_EQ(result.final_account.equity, Money::from_integer(100'120));
}

TEST(Engine, LongTakeProfitClosesAtBid) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    const TickSeries ticks{
        tick(1, id, "1.099800", "1.100000"),
        tick(2, id, "1.099800", "1.100000"),
        tick(3, id, "1.111000", "1.111200")};
    EngineConfig config;
    config.account.risk.take_profit_bps = 100;
    BuyAndHold strategy(id, Quantity::from_integer(10'000));
    const auto result = run(strategy, ticks, registry, config);
    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[1].reason, FillReason::take_profit);
    EXPECT_EQ(result.trades[1].price, Price::parse("1.111000"));
    EXPECT_EQ(result.trades[1].realized_pnl, Money::from_integer(110));
}

TEST(Engine, ShortStopLossCoversAtAsk) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    const TickSeries ticks{
        tick(1, id, "1.100000", "1.100200"),
        tick(2, id, "1.100000", "1.100200"),
        tick(3, id, "1.111000", "1.111200")};
    EngineConfig config;
    config.account.risk.stop_loss_bps = 100;
    BuyAndHold strategy(id, Quantity::from_integer(10'000), Side::sell);
    const auto result = run(strategy, ticks, registry, config);
    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[1].reason, FillReason::stop_loss);
    EXPECT_EQ(result.trades[1].price, Price::parse("1.111200"));
    EXPECT_EQ(result.trades[1].realized_pnl, Money::from_integer(-112));
}

TEST(Engine, EmptyReplayReturnsEmptyResult) {
    InstrumentRegistry registry;
    const auto id = registry.ensure_fx("EURUSD");
    BuyAndHold strategy(id, Quantity::from_integer(1));
    const auto result = run(strategy, {}, registry, EngineConfig{});
    EXPECT_EQ(result.ticks_processed, 0u);
    EXPECT_TRUE(result.trades.empty());
}
