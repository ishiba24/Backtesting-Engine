#pragma once

#include "backtest/types.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace backtest {

class RollingSma {
public:
    explicit RollingSma(std::size_t period)
        : period_(period), buffer_(period) {
        if (period == 0) {
            throw std::invalid_argument("SMA period must be positive");
        }
    }

    void push(Price value) {
        const std::size_t slot = count_ % period_;
        if (count_ < period_) {
            sum_ += value;
        } else {
            sum_ += value - buffer_[slot];
        }
        buffer_[slot] = value;
        ++count_;
    }

    bool ready() const { return count_ >= period_; }
    Price value() const {
        if (!ready()) {
            throw std::logic_error("SMA is not ready");
        }
        return sum_ / Decimal::from_integer(static_cast<std::int64_t>(period_));
    }

private:
    std::size_t period_;
    std::vector<Price> buffer_;
    Price sum_;
    std::size_t count_ = 0;
};

}  // namespace backtest
