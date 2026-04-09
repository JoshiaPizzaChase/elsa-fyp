#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>

#include "noise_trader.h"
#include "simulation_config.h"
#include "test_client.h"

int main() {
    NoiseTradersConfig config; // Assuming defaults from configuration struct

    std::cout << "Deploying " << config.num_noise_traders << " Noise Traders..." << '\n';

    std::vector<std::shared_ptr<NoiseTrader>> traders;
    std::vector<std::thread> trader_threads;

    for (int i = 0; i < config.num_noise_traders; ++i) {
        // Construct the config filename based on the specified prefix and index.
        std::string config_path = config.cfg_prefix + "_" + std::to_string(i) + ".cfg";

        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Warning: Expected FIX client config file does not exist at " << config_path << '\n';
            // Continue anyway to let FixClient (QuickFix) throw an exception or handle it
        }

        // Instantiate the random distribution generators based on the configuration parameters.
        auto process_gen = std::make_unique<PoissonProcessGenerator>(config.lambda_eps);
        auto decision_gen = std::make_unique<BernoulliDecisionGenerator<OrderSide>>(config.bernoulli);
        auto quantity_gen = std::make_unique<ParetoQuantityGenerator<double>>(config.pareto_scale, config.pareto_shape);

        auto fix_client = std::make_unique<TestClient>(config_path);

        try {
            fix_client->connect(config.server_port);
        } catch(const std::exception& e) {
            std::cerr << "Error connecting FixClient " << i << ": " << e.what() << '\n';
            continue;
        }

        // Create the noise trader instance.
        auto trader = std::make_shared<NoiseTrader>(
            std::move(process_gen),
            std::move(decision_gen),
            std::move(quantity_gen),
            std::move(fix_client),
            std::move(config.tickers)
        );

        traders.push_back(trader);

        // Spawn a background thread to run the trading loop indefinitely.
        trader_threads.emplace_back([trader]() {
            trader->run_strategy();
        });
    }

    // Keep the main deployment thread alive so the spawned noise trader threads can run asynchronously.
    std::cout << "All traders deployed and running. Press Ctrl+C to terminate." << '\n';

    for (auto& t : trader_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return 0;
}
