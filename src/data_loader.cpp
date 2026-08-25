#include "backtest/data_loader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace backtest {
namespace {

std::vector<std::string> split_csv_row(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream input(line);
    std::string field;
    while (std::getline(input, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

Tick parse_tick(const std::string& line, InstrumentRegistry& registry) {
    const auto fields = split_csv_row(line);
    if (fields.size() != 6) {
        throw std::runtime_error("tick row must contain exactly six fields: " + line);
    }
    Tick tick;
    tick.ts = std::stoll(fields[0]);
    tick.instrument_id = registry.ensure_fx(fields[1]);
    tick.bid = Price::parse(fields[2]);
    tick.ask = Price::parse(fields[3]);
    tick.bid_size = Quantity::parse(fields[4]);
    tick.ask_size = Quantity::parse(fields[5]);
    if (tick.ts < 0 || tick.bid <= Price{} || tick.ask < tick.bid ||
        tick.bid_size < Quantity{} || tick.ask_size < Quantity{}) {
        throw std::runtime_error("invalid quote values: " + line);
    }
    return tick;
}

}  // namespace

TickSeries load_ticks_csv(const std::string& path, InstrumentRegistry& registry) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("failed to open tick CSV: " + path);
    }
    std::string line;
    if (!std::getline(file, line)) {
        throw std::runtime_error("tick CSV is empty: " + path);
    }
    if (line != "timestamp_ns,symbol,bid,ask,bid_size,ask_size") {
        throw std::runtime_error("unexpected tick CSV header: " + line);
    }

    TickSeries ticks;
    ticks.reserve(1 << 16);
    std::unordered_map<InstrumentId, Timestamp> last_by_instrument;
    Timestamp last_global = -1;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        Tick tick = parse_tick(line, registry);
        if (tick.ts < last_global) {
            throw std::runtime_error("ticks are not globally ordered by timestamp");
        }
        if (const auto it = last_by_instrument.find(tick.instrument_id);
            it != last_by_instrument.end() && tick.ts <= it->second) {
            throw std::runtime_error("instrument ticks are not strictly increasing");
        }
        last_global = tick.ts;
        last_by_instrument[tick.instrument_id] = tick.ts;
        ticks.push_back(tick);
    }
    return ticks;
}

std::span<const Tick> slice_by_time(
    std::span<const Tick> ticks, Timestamp start, Timestamp end) {
    const auto begin = std::lower_bound(
        ticks.begin(), ticks.end(), start,
        [](const Tick& tick, Timestamp ts) { return tick.ts < ts; });
    const auto finish = std::lower_bound(
        ticks.begin(), ticks.end(), end,
        [](const Tick& tick, Timestamp ts) { return tick.ts < ts; });
    return ticks.subspan(
        static_cast<std::size_t>(begin - ticks.begin()),
        static_cast<std::size_t>(finish - begin));
}

}  // namespace backtest
