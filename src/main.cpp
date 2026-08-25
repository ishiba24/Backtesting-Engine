#include "backtest/data_loader.hpp"
#include "backtest/engine.hpp"
#include "backtest/metrics.hpp"
#include "backtest/strategies/buy_and_hold.hpp"
#include "backtest/strategies/sma_crossover.hpp"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace {

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " --data <tick.csv> [options]\n"
        << "  --strategy buy_and_hold|sma_crossover\n"
        << "  --symbol EURUSD --quantity 10000 --direction long|short\n"
        << "  --fast 20 --slow 50 --long-only\n"
        << "  --cash 100000 --leverage 30 --quote-to-account 1\n"
        << "  --fee-bps 0 --min-fee 0 --slippage-bps 0\n"
        << "  --stop-loss-bps 100 --take-profit-bps 200\n"
        << "  --start YYYY-MM-DD --end YYYY-MM-DD\n"
        << "  --log-equity <csv> --log-trades <csv> --summary <json>\n";
}

bool parse_date_ns(const std::string& value, backtest::Timestamp& timestamp) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(value.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    std::tm time{};
    time.tm_year = year - 1900;
    time.tm_mon = month - 1;
    time.tm_mday = day;
    const std::time_t seconds = timegm(&time);
    if (seconds < 0) {
        return false;
    }
    timestamp = static_cast<backtest::Timestamp>(seconds) * 1'000'000'000LL;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace backtest;

    std::string data_path;
    std::string strategy_name = "buy_and_hold";
    std::string symbol = "EURUSD";
    std::string direction = "long";
    std::string equity_path;
    std::string trades_path;
    std::string summary_path;
    Quantity quantity = Quantity::from_integer(10'000);
    Money cash = Money::from_integer(100'000);
    Decimal leverage = Decimal::from_integer(30);
    Decimal quote_to_account = Decimal::from_integer(1);
    int fast = 20;
    int slow = 50;
    std::uint32_t fee_bps = 0;
    Money min_fee;
    std::uint32_t slippage_bps = 0;
    std::uint32_t stop_loss_bps = 0;
    std::uint32_t take_profit_bps = 0;
    bool allow_short = true;
    bool have_start = false;
    bool have_end = false;
    Timestamp start = 0;
    Timestamp end = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        auto value = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(std::string("missing value for ") + flag);
            }
            return argv[++i];
        };

        try {
            if (argument == "--data") data_path = value("--data");
            else if (argument == "--strategy") strategy_name = value("--strategy");
            else if (argument == "--symbol") symbol = value("--symbol");
            else if (argument == "--quantity") quantity = Quantity::parse(value("--quantity"));
            else if (argument == "--direction") direction = value("--direction");
            else if (argument == "--fast") fast = std::stoi(value("--fast"));
            else if (argument == "--slow") slow = std::stoi(value("--slow"));
            else if (argument == "--cash") cash = Money::parse(value("--cash"));
            else if (argument == "--leverage") leverage = Decimal::parse(value("--leverage"));
            else if (argument == "--quote-to-account") quote_to_account = Decimal::parse(value("--quote-to-account"));
            else if (argument == "--fee-bps") fee_bps = std::stoul(value("--fee-bps"));
            else if (argument == "--min-fee") min_fee = Money::parse(value("--min-fee"));
            else if (argument == "--slippage-bps") slippage_bps = std::stoul(value("--slippage-bps"));
            else if (argument == "--stop-loss-bps") stop_loss_bps = std::stoul(value("--stop-loss-bps"));
            else if (argument == "--take-profit-bps") take_profit_bps = std::stoul(value("--take-profit-bps"));
            else if (argument == "--start") {
                have_start = parse_date_ns(value("--start"), start);
                if (!have_start) throw std::invalid_argument("invalid start date");
            } else if (argument == "--end") {
                have_end = parse_date_ns(value("--end"), end);
                if (!have_end) throw std::invalid_argument("invalid end date");
            } else if (argument == "--log-equity") equity_path = value("--log-equity");
            else if (argument == "--log-trades") trades_path = value("--log-trades");
            else if (argument == "--summary") summary_path = value("--summary");
            else if (argument == "--long-only") allow_short = false;
            else if (argument == "--help" || argument == "-h") {
                usage(argv[0]);
                return 0;
            } else {
                throw std::invalid_argument("unknown argument: " + argument);
            }
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            usage(argv[0]);
            return 1;
        }
    }

    if (data_path.empty()) {
        usage(argv[0]);
        return 1;
    }
    if (direction != "long" && direction != "short") {
        std::cerr << "direction must be long or short\n";
        return 1;
    }

    try {
        InstrumentRegistry registry;
        const InstrumentId target_id = registry.ensure_fx(symbol);
        if (quote_to_account <= Decimal{}) {
            throw std::invalid_argument("quote-to-account rate must be positive");
        }
        registry.get_mutable(target_id).quote_to_account_rate = quote_to_account;
        TickSeries ticks = load_ticks_csv(data_path, registry);
        if (ticks.empty()) {
            throw std::runtime_error("tick CSV contains no data rows");
        }
        std::span<const Tick> replay = ticks;
        if (have_start || have_end) {
            replay = slice_by_time(
                replay,
                have_start ? start : replay.front().ts,
                have_end ? end : replay.back().ts + 1);
        }
        if (replay.empty()) {
            throw std::runtime_error("no ticks in requested replay window");
        }

        EngineConfig config;
        config.account.starting_balance = cash;
        config.account.leverage = leverage;
        config.account.fee_bps = fee_bps;
        config.account.min_fee = min_fee;
        config.account.slippage_bps = slippage_bps;
        config.account.risk.stop_loss_bps = stop_loss_bps;
        config.account.risk.take_profit_bps = take_profit_bps;

        std::unique_ptr<IStrategy> strategy;
        if (strategy_name == "buy_and_hold") {
            strategy = std::make_unique<BuyAndHold>(
                target_id, quantity,
                direction == "long" ? Side::buy : Side::sell);
        } else if (strategy_name == "sma_crossover") {
            strategy = std::make_unique<SmaCrossover>(
                target_id, static_cast<std::size_t>(fast),
                static_cast<std::size_t>(slow), quantity, allow_short);
        } else {
            throw std::invalid_argument("unknown strategy: " + strategy_name);
        }

        const RunResult result = run(*strategy, replay, registry, config);
        const Metrics metrics = compute_metrics(result, cash, strategy->name());
        print_metrics(metrics, registry);

        if (!equity_path.empty() && !write_equity_csv(equity_path, result.equity_curve)) {
            throw std::runtime_error("failed to write equity CSV");
        }
        if (!trades_path.empty() && !write_trades_csv(trades_path, result.trades, registry)) {
            throw std::runtime_error("failed to write trades CSV");
        }
        if (!summary_path.empty() && !write_summary_json(summary_path, metrics)) {
            throw std::runtime_error("failed to write summary JSON");
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
