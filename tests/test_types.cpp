#include "backtest/types.hpp"

#include <gtest/gtest.h>

using namespace backtest;

TEST(Decimal, ParsesAndFormatsExactly) {
    EXPECT_EQ(Decimal::parse("1.234567").raw(), 1'234'567);
    EXPECT_EQ(Decimal::parse("-12.5").to_string(), "-12.500000");
    EXPECT_THROW(Decimal::parse("1.0000001"), std::invalid_argument);
}

TEST(Decimal, MultipliesAndDividesAtFixedScale) {
    EXPECT_EQ((Decimal::parse("1.250000") * Decimal::parse("2.000000")),
              Decimal::parse("2.500000"));
    EXPECT_EQ((Decimal::parse("2.500000") / Decimal::parse("2.000000")),
              Decimal::parse("1.250000"));
}

TEST(Decimal, AppliesBasisPointsWithoutFloatingPoint) {
    EXPECT_EQ(apply_bps(Price::parse("1.100000"), 100), Price::parse("1.111000"));
    EXPECT_EQ(apply_bps(Price::parse("1.100000"), -100), Price::parse("1.089000"));
}
