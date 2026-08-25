#include "backtest/data_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace {
std::filesystem::path fixture() {
    return std::filesystem::path(__FILE__).parent_path() /
        "fixtures" / "eurusd_ticks.csv";
}
}  // namespace

TEST(DataLoader, LoadsExactBidAskTicksAndRegistry) {
    backtest::InstrumentRegistry registry;
    const auto ticks = backtest::load_ticks_csv(fixture().string(), registry);
    ASSERT_EQ(ticks.size(), 8u);
    EXPECT_EQ(registry.size(), 1u);
    EXPECT_EQ(registry.get(ticks.front().instrument_id).symbol, "EURUSD");
    EXPECT_EQ(ticks.front().bid, backtest::Price::parse("1.100000"));
    EXPECT_EQ(ticks.front().ask, backtest::Price::parse("1.100200"));
}

TEST(DataLoader, SliceUsesHalfOpenNanosecondRangeWithoutCopying) {
    backtest::InstrumentRegistry registry;
    const auto ticks = backtest::load_ticks_csv(fixture().string(), registry);
    const auto sliced = backtest::slice_by_time(ticks, 2, 5);
    ASSERT_EQ(sliced.size(), 3u);
    EXPECT_EQ(sliced.front().ts, 2);
    EXPECT_EQ(sliced.back().ts, 4);
    EXPECT_EQ(&sliced.front(), &ticks[1]);
}

TEST(DataLoader, MissingFileThrows) {
    backtest::InstrumentRegistry registry;
    EXPECT_THROW(backtest::load_ticks_csv("missing.csv", registry), std::runtime_error);
}
