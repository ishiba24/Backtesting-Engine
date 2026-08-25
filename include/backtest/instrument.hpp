#pragma once

#include "backtest/types.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace backtest {

struct Instrument {
    InstrumentId id = 0;
    std::string symbol;
    std::string base_currency;
    std::string quote_currency;
    Price tick_size = Price::from_raw(1);
    Decimal contract_size = Decimal::from_integer(1);
    Decimal quote_to_account_rate = Decimal::from_integer(1);
};

class InstrumentRegistry {
public:
    InstrumentId add(Instrument instrument);
    InstrumentId ensure_fx(std::string_view symbol);
    const Instrument& get(InstrumentId id) const;
    Instrument& get_mutable(InstrumentId id);
    const Instrument& get(std::string_view symbol) const;
    InstrumentId id_for(std::string_view symbol) const;
    std::size_t size() const { return instruments_.size(); }
    std::span<const Instrument> instruments() const { return instruments_; }

private:
    std::vector<Instrument> instruments_;
    std::unordered_map<std::string, InstrumentId> symbol_to_id_;
};

}  // namespace backtest
