#include "backtest/types.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace backtest {
namespace {

std::int64_t checked_i128(__int128 value) {
    if (value > std::numeric_limits<std::int64_t>::max() ||
        value < std::numeric_limits<std::int64_t>::min()) {
        throw std::overflow_error("fixed-point decimal overflow");
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace

Decimal Decimal::from_double(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("decimal must be finite");
    }
    const long double scaled = static_cast<long double>(value) * scale;
    if (scaled > std::numeric_limits<std::int64_t>::max() ||
        scaled < std::numeric_limits<std::int64_t>::min()) {
        throw std::overflow_error("fixed-point decimal overflow");
    }
    return from_raw(static_cast<std::int64_t>(std::llround(scaled)));
}

Decimal Decimal::parse(std::string_view value) {
    if (value.empty()) {
        throw std::invalid_argument("empty decimal");
    }
    bool negative = false;
    std::size_t pos = 0;
    if (value[pos] == '+' || value[pos] == '-') {
        negative = value[pos] == '-';
        ++pos;
    }
    if (pos == value.size()) {
        throw std::invalid_argument("invalid decimal");
    }

    __int128 whole = 0;
    bool have_digit = false;
    while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') {
        have_digit = true;
        whole = whole * 10 + (value[pos++] - '0');
    }

    std::int64_t fraction = 0;
    int fraction_digits = 0;
    if (pos < value.size() && value[pos] == '.') {
        ++pos;
        while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') {
            if (fraction_digits == 6) {
                throw std::invalid_argument("decimal has more than six fractional digits");
            }
            fraction = fraction * 10 + (value[pos++] - '0');
            ++fraction_digits;
            have_digit = true;
        }
    }
    if (!have_digit || pos != value.size()) {
        throw std::invalid_argument("invalid decimal");
    }
    while (fraction_digits++ < 6) {
        fraction *= 10;
    }

    __int128 raw = whole * scale + fraction;
    if (negative) {
        raw = -raw;
    }
    return from_raw(checked_i128(raw));
}

double Decimal::to_double() const {
    return static_cast<double>(raw_) / static_cast<double>(scale);
}

std::string Decimal::to_string() const {
    const bool negative = raw_ < 0;
    const std::uint64_t magnitude = negative
        ? static_cast<std::uint64_t>(-(raw_ + 1)) + 1
        : static_cast<std::uint64_t>(raw_);
    std::ostringstream out;
    if (negative) {
        out << '-';
    }
    out << magnitude / scale << '.' << std::setw(6) << std::setfill('0')
        << magnitude % scale;
    return out.str();
}

Decimal operator*(Decimal lhs, Decimal rhs) {
    const __int128 product = static_cast<__int128>(lhs.raw_) * rhs.raw_;
    return Decimal::from_raw(checked_i128(product / Decimal::scale));
}

Decimal operator/(Decimal lhs, Decimal rhs) {
    if (rhs.raw_ == 0) {
        throw std::domain_error("fixed-point division by zero");
    }
    const __int128 numerator = static_cast<__int128>(lhs.raw_) * Decimal::scale;
    return Decimal::from_raw(checked_i128(numerator / rhs.raw_));
}

Decimal apply_bps(Decimal value, std::int32_t bps) {
    const __int128 numerator = static_cast<__int128>(value.raw()) * (10'000 + bps);
    return Decimal::from_raw(checked_i128(numerator / 10'000));
}

std::string_view to_string(Side side) {
    return side == Side::buy ? "buy" : "sell";
}

std::string_view to_string(FillReason reason) {
    switch (reason) {
        case FillReason::strategy: return "strategy";
        case FillReason::stop_loss: return "stop_loss";
        case FillReason::take_profit: return "take_profit";
    }
    return "unknown";
}

}  // namespace backtest
