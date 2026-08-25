#include "backtest/engine.hpp"
#include "backtest/strategies/buy_and_hold.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <vector>

int main(int argc, char** argv) {
    using namespace backtest;
    const std::size_t count = argc > 1
        ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10))
        : 1'000'000;
    const int repetitions = argc > 2 ? std::atoi(argv[2]) : 5;

    InstrumentRegistry registry;
    const InstrumentId id = registry.ensure_fx("EURUSD");
    std::vector<Tick> ticks;
    ticks.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto wave = static_cast<std::int64_t>(i % 2'000) - 1'000;
        const Price bid = Price::from_raw(1'100'000 + wave);
        ticks.push_back(Tick{
            .ts = static_cast<Timestamp>(i),
            .instrument_id = id,
            .bid = bid,
            .ask = bid + Price::from_raw(100),
            .bid_size = Quantity::from_integer(1'000'000),
            .ask_size = Quantity::from_integer(1'000'000),
        });
    }

    EngineConfig config;
    config.log_equity_curve = false;
    config.log_trades = false;
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repetitions));
    std::size_t processed = 0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        BuyAndHold strategy(id, Quantity::from_integer(10'000));
        const auto start = std::chrono::steady_clock::now();
        const RunResult result = run(strategy, ticks, registry, config);
        const auto finish = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(finish - start).count();
        samples.push_back(seconds);
        processed = result.ticks_processed;
    }
    std::sort(samples.begin(), samples.end());
    const double median_seconds = samples[samples.size() / 2];

    std::cout << std::fixed << std::setprecision(2)
              << "ticks=" << processed
              << " repetitions=" << repetitions
              << " median_seconds=" << median_seconds
              << " ticks_per_second=" << processed / median_seconds
              << " ns_per_tick=" << median_seconds * 1e9 / processed << '\n';
    return processed == count ? 0 : 1;
}
