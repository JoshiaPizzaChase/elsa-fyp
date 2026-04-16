#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>
#include <format>

#include "config.h"
#include "noise_trader.h"
#include "market_maker.h"
#include "simulation_config.h"
#include "test_client.h"
#include "rfl/toml/load.hpp"
#include "logger/logger.h"
#include <transport/websocket/websocket_client.h>

using namespace simulation;

int main() {
    const auto nt_config_path = "nt_config.toml";
    const simulation::NoiseTradersConfig config = rfl::toml::load<simulation::NoiseTradersConfig>(nt_config_path).value();
    const std::string log_dir =
        std::format("{}/logs/{}", std::string(PROJECT_SOURCE_DIR), SERVER_NAME);
    std::filesystem::create_directories(log_dir);
    auto noise_trader_logger = logger::create_logger(
        std::format("noise_trader_{}_logger", config.cfg_prefix),
        std::format("{}/{}.log", log_dir, config.cfg_prefix));

    std::cout << "Deploying " << config.num_noise_traders << " Noise Traders..." << '\n';
    noise_trader_logger->info("Deploying {} Noise Traders...", config.num_noise_traders);

    std::vector<std::shared_ptr<NoiseTrader>> traders;
    std::vector<std::thread> trader_threads;
    std::vector<std::shared_ptr<transport::WebsocketManagerClient>> ws_clients;
    std::vector<std::shared_ptr<MarketDataHandler>> md_handlers;

    for (int i = 0; i < config.num_noise_traders; ++i) {
        // Construct the config filename based on the specified prefix and index.
        std::string config_path = config.cfg_prefix + "_" + std::to_string(i + 1) + ".cfg";

        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Warning: Expected FIX client config file does not exist at " << config_path << '\n';
            noise_trader_logger->warn(
                "Expected FIX client config file does not exist at {}", config_path);
            // Continue anyway to let FixClient (QuickFix) throw an exception or handle it
        }

        // Instantiate the random distribution generators based on the configuration parameters.
        auto process_gen = std::make_unique<PoissonProcessGenerator>(config.lambda_eps);
        auto decision_gen = std::make_unique<BernoulliDecisionGenerator<OrderSide>>(config.bernoulli);
        auto quantity_gen = std::make_unique<ParetoQuantityGenerator<double>>(config.pareto_scale, config.pareto_shape);

        auto ws_client = std::make_shared<transport::WebsocketManagerClient>(config.cfg_prefix + std::to_string(i + 1) + "_logger");
        ws_client->start();
        ws_client->connect("ws://" + config.mdp_ws_host + ":" + std::to_string(config.mdp_ws_port));
        ws_clients.push_back(ws_client);

        auto md_handler = std::make_shared<MarketDataHandler>(ws_client, noise_trader_logger);
        md_handler->start();
        md_handlers.push_back(md_handler);

        auto fix_client = std::make_unique<TestClient>(config_path);

        try {
            fix_client->connect(5);
        } catch(const std::exception& e) {
            std::cerr << "Error connecting FixClient " << i << ": " << e.what() << '\n';
            noise_trader_logger->error("Error connecting FixClient {}: {}", i, e.what());
            continue;
        }

        // Create the noise trader instance.
        auto trader = std::make_shared<NoiseTrader>(
            std::move(process_gen),
            std::move(decision_gen),
            std::move(quantity_gen),
            std::move(fix_client),
            std::move(config.tickers),
            [md_handler](const std::string& ticker) {
                const auto recent_trades = md_handler->get_recent_trades(ticker);
                if (!recent_trades.empty()) {
                    return recent_trades.back().price;
                }
                return 100.0;
            },
            noise_trader_logger
        );

        traders.push_back(trader);

        // Spawn a background thread to run the trading loop indefinitely.
        trader_threads.emplace_back([trader]() {
            trader->run_strategy();
        });
    }

    // Keep the main deployment thread alive so the spawned noise trader threads can run asynchronously.
    std::cout << "All traders deployed and running. Press Ctrl+C to terminate." << '\n';
    noise_trader_logger->info("All noise traders deployed and running.");

    for (auto& t : trader_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return 0;
}
