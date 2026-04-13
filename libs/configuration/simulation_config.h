#pragma once

#include <string>
#include <vector>

namespace simulation {
    struct NoiseTradersConfig {
        const std::string cfg_prefix{"noise_trader"};
        const int num_noise_traders{5};
        const double lambda_eps{20.0};
        const double bernoulli{0.5};
        const double pareto_scale{1.0};
        const double pareto_shape{2.0};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct MarketMakersConfig {
        const std::string cfg_prefix{"market_maker"};
        const int num_market_makers{1};
        const double lot_size{1.0};
        const double gamma{0.1};
        const double k{1.5};
        const double terminal_time{1.0};
        const double initial_inventory{100.0};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct InformedTradersConfig {
        const std::string cfg_prefix{"informed_trader"};
        const int num_informed_traders{1};
        const double initial_inventory{100.0};
        const double epsilon{0.05};
        const double trade_qty{10.0};
        const double max_inventory{1000.0};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct OracleServerConfig {
        const double mu{0.0};
        const double sigma{0.05};
        const double jump_intensity{0.5};
        const double jump_mean{0.0};
        const double jump_std{0.02};
        const int update_interval_ms{10};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };
}
