#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>

#include "../traders/market_maker.h"
#include "simulation_config.h"
#include "rfl/toml/load.hpp"
#include <transport/websocket/websocket_client.h>

using namespace simulation;

int main() {
    const auto mm_config_path = "mm_config.toml";
    const simulation::MarketMakersConfig config = rfl::toml::load<simulation::MarketMakersConfig>(mm_config_path).value();

    std::cout << "Deploying " << config.num_market_makers << " Market Makers..." << '\n';

    std::vector<std::shared_ptr<transport::WebsocketManagerClient>> ws_clients;
    std::vector<std::shared_ptr<MarketDataHandler>> md_handlers;
    std::vector<std::shared_ptr<MarketMaker>> traders;

    for (int i = 0; i < config.num_market_makers; ++i) {
        // Construct the config filename based on the specified prefix and index.
        std::string config_path = config.cfg_prefix + "_" + std::to_string(i + 1) + ".cfg";

        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Warning: Expected FIX client config file does not exist at " << config_path << '\n';
            // Continue anyway to let FixClient (QuickFix) throw an exception or handle it
        }

        // Create and store the websocket client
        auto ws_client = std::make_shared<transport::WebsocketManagerClient>(config.cfg_prefix + std::to_string(i + 1) + "_logger");
        ws_client->start();
        ws_client->connect("ws://localhost:9001");
        ws_clients.push_back(ws_client);

        // Create and start the market data handler
        auto md_handler = std::make_shared<MarketDataHandler>(ws_client);
        md_handler->start();
        md_handlers.push_back(md_handler);

        // Create the FIX client for this market maker
        auto fix_client = std::make_shared<MMFixClient>(config_path);

        try {
            fix_client->connect(5);
        } catch(const std::exception& e) {
            std::cerr << "Error connecting MMFixClient " << i << ": " << e.what() << '\n';
            continue;
        }

        // Create the market maker instance
        auto trader = std::make_shared<MarketMaker>(
            md_handler,
            fix_client,
            config.initial_inventory,
            config.lot_size,
            config.gamma,
            config.k,
            config.terminal_time
        );

        traders.push_back(trader);

        // Start the background trading loop
        trader->start(config.tickers);
    }

    // Keep the main deployment thread alive so the spawned market maker threads can run asynchronously.
    std::cout << "All traders deployed and running. Press Ctrl+C to terminate." << '\n';

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}
