#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace backtest {

using Timestamp = std::int64_t;  // Unix time in nanoseconds.
using InstrumentId = std::uint32_t;

// Signed fixed-point decimal with six fractional digits. Prices, quantities,
// cash and PnL all use the same exact representation in the simulation.
class Decimal {
public:
    static constexpr std::int64_t scale = 1'000'000;

    constexpr Decimal() = default;

    static constexpr Decimal from_raw(std::int64_t raw) {
        return Decimal(raw);
    }

    static constexpr Decimal from_integer(std::int64_t value) {
        return Decimal(value * scale);
    }

    static Decimal from_double(double value);
    static Decimal parse(std::string_view value);

    constexpr std::int64_t raw() const { return raw_; }
    double to_double() const;
    std::string to_string() const;

    constexpr auto operator<=>(const Decimal&) const = default;

    constexpr Decimal operator-() const { return from_raw(-raw_); }
    constexpr Decimal& operator+=(Decimal rhs) {
        raw_ += rhs.raw_;
        return *this;
    }
    constexpr Decimal& operator-=(Decimal rhs) {
        raw_ -= rhs.raw_;
        return *this;
    }

    friend constexpr Decimal operator+(Decimal lhs, Decimal rhs) {
        lhs += rhs;
        return lhs;
    }
    friend constexpr Decimal operator-(Decimal lhs, Decimal rhs) {
        lhs -= rhs;
        return lhs;
    }
    friend Decimal operator*(Decimal lhs, Decimal rhs);
    friend Decimal operator/(Decimal lhs, Decimal rhs);

private:
    explicit constexpr Decimal(std::int64_t raw) : raw_(raw) {}
    std::int64_t raw_ = 0;
};

using Price = Decimal;
using Quantity = Decimal;
using Money = Decimal;

constexpr Decimal abs(Decimal value) {
    return value.raw() < 0 ? -value : value;
}

Decimal apply_bps(Decimal value, std::int32_t bps);

enum class Side { buy, sell };
enum class OrderType { market };
enum class FillReason { strategy, stop_loss, take_profit };

constexpr int side_sign(Side side) {
    return side == Side::buy ? 1 : -1;
}

std::string_view to_string(Side side);
std::string_view to_string(FillReason reason);

}  // namespace backtest
