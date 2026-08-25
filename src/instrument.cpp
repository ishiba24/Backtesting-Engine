#include "backtest/instrument.hpp"

#include <stdexcept>

namespace backtest {

InstrumentId InstrumentRegistry::add(Instrument instrument) {
    if (instrument.symbol.empty()) {
        throw std::invalid_argument("instrument symbol cannot be empty");
    }
    if (instrument.tick_size <= Price{} ||
        instrument.contract_size <= Decimal{} ||
        instrument.quote_to_account_rate <= Decimal{}) {
        throw std::invalid_argument("instrument scales and conversion must be positive");
    }
    if (symbol_to_id_.contains(instrument.symbol)) {
        throw std::invalid_argument("duplicate instrument: " + instrument.symbol);
    }
    instrument.id = static_cast<InstrumentId>(instruments_.size());
    const InstrumentId id = instrument.id;
    symbol_to_id_.emplace(instrument.symbol, id);
    instruments_.push_back(std::move(instrument));
    return id;
}

InstrumentId InstrumentRegistry::ensure_fx(std::string_view symbol) {
    if (const auto it = symbol_to_id_.find(std::string(symbol)); it != symbol_to_id_.end()) {
        return it->second;
    }
    if (symbol.size() != 6) {
        throw std::invalid_argument("FX symbols must use six characters, e.g. EURUSD");
    }
    Instrument instrument;
    instrument.symbol = std::string(symbol);
    instrument.base_currency = std::string(symbol.substr(0, 3));
    instrument.quote_currency = std::string(symbol.substr(3, 3));
    instrument.tick_size = Price::parse("0.000001");
    instrument.contract_size = Decimal::from_integer(1);
    instrument.quote_to_account_rate = Decimal::from_integer(1);
    return add(std::move(instrument));
}

const Instrument& InstrumentRegistry::get(InstrumentId id) const {
    if (id >= instruments_.size()) {
        throw std::out_of_range("unknown instrument id");
    }
    return instruments_[id];
}

Instrument& InstrumentRegistry::get_mutable(InstrumentId id) {
    if (id >= instruments_.size()) {
        throw std::out_of_range("unknown instrument id");
    }
    return instruments_[id];
}

InstrumentId InstrumentRegistry::id_for(std::string_view symbol) const {
    const auto it = symbol_to_id_.find(std::string(symbol));
    if (it == symbol_to_id_.end()) {
        throw std::out_of_range("unknown instrument: " + std::string(symbol));
    }
    return it->second;
}

const Instrument& InstrumentRegistry::get(std::string_view symbol) const {
    return get(id_for(symbol));
}

}  // namespace backtest
