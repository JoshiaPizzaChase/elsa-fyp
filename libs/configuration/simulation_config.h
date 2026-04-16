#pragma once

#include <string>
#include <vector>

namespace simulation {
    struct NoiseTradersConfig {
        const std::string cfg_prefix{"noise_trader"};
        const int num_noise_traders{3};
        const double lambda_eps{0.6};
        const double bernoulli{0.5};
        const double pareto_scale{0.5};
        const double pareto_shape{4.0};
        const double initial_inventory{20.0};
        const std::string mdp_ws_host{"localhost"};
        const int mdp_ws_port{9001};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct MarketMakersConfig {
        const std::string cfg_prefix{"market_maker"};
        const int num_market_makers{4};
        const double lot_size{1.0};
        const double gamma{0.6};
        const double k{8.0};
        const double terminal_time{30.0};
        const double initial_inventory{300.0};
        const std::string mdp_ws_host{"localhost"};
        const int mdp_ws_port{9001};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct InformedTradersConfig {
        const std::string cfg_prefix{"informed_trader"};
        const int num_informed_traders{1};
        const double initial_inventory{100.0};
        const double epsilon{0.05};
        const double trade_qty{10.0};
        const double max_inventory{1000.0};
        const std::string mdp_ws_host{"localhost"};
        const int mdp_ws_port{9001};
        const std::string oracle_ws_host{"localhost"};
        const int oracle_ws_port{9005};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };

    struct OracleServerConfig {
        const double mu{0.0};
        const double sigma{0.003};
        const double jump_intensity{0.06};
        const double jump_mean{0.0};
        const double jump_std{0.01};
        const int update_interval_ms{60000};
        const int oracle_ws_port{9005};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };
}
