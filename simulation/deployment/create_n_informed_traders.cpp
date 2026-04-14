#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>

#include "../traders/informed_trader.h"
#include "../traders/oracle_service.h"
#include "simulation_config.h"
#include "rfl/toml/load.hpp"
#include <transport/websocket/websocket_client.h>

using namespace simulation;

int main() {
    const auto it_config_path = "it_config.toml";

    simulation::InformedTradersConfig config = rfl::toml::load<simulation::InformedTradersConfig>(it_config_path).value();

    std::cout << "Deploying " << config.num_informed_traders << " Informed Traders..." << '\n';

    std::vector<std::shared_ptr<transport::WebsocketManagerClient>> md_ws_clients;
    std::vector<std::shared_ptr<transport::WebsocketManagerClient>> oracle_ws_clients;
    std::vector<std::shared_ptr<MarketDataHandler>> md_handlers;
    std::vector<std::shared_ptr<OracleClient>> oracle_clients;
    std::vector<std::shared_ptr<InformedTrader>> traders;

    for (int i = 0; i < config.num_informed_traders; ++i) {
        // Construct the config filename based on the specified prefix and index.
        std::string config_path = config.cfg_prefix + "_" + std::to_string(i + 1) + ".cfg";

        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Warning: Expected FIX client config file does not exist at " << config_path << '\n';
        }

        // 1. Market Data Websocket & Handler (Connects to Exchange)
        auto md_ws_client = std::make_shared<transport::WebsocketManagerClient>(config.cfg_prefix + std::to_string(i + 1) + "_logger");
        md_ws_client->start();
        md_ws_client->connect("ws://localhost:9001");

        md_ws_clients.push_back(md_ws_client);

        auto md_handler = std::make_shared<MarketDataHandler>(md_ws_client);
        md_handler->start();
        md_handlers.push_back(md_handler);

        // 2. Oracle Websocket & Client (Connects to Fundamental Price Oracle)
        auto oracle_ws_client = std::make_shared<transport::WebsocketManagerClient>("informed_t_for_cl" + std::to_string(i+1));
        oracle_ws_client->start();
        oracle_ws_client->connect("ws://localhost:9005");
        oracle_ws_clients.push_back(oracle_ws_client);

        auto oracle_client = std::make_shared<OracleClient>(oracle_ws_client);
        oracle_client->start();
        oracle_clients.push_back(oracle_client);

        // 3. FIX Client (For Order Execution)
        auto fix_client = std::make_shared<InformedFixClient>(config_path);
        
        try {
            fix_client->connect(5);
        } catch(const std::exception& e) {
            std::cerr << "Error connecting InformedFixClient " << i << ": " << e.what() << '\n';
            continue;
        }

        // 4. Informed Trader Agent
        auto trader = std::make_shared<InformedTrader>(
            md_handler,
            oracle_client,
            fix_client,
            config.initial_inventory,
            config.epsilon,
            config.trade_qty,
            config.max_inventory
        );

        traders.push_back(trader);

        // Start the background trading loop
        trader->start(config.tickers);
    }

    // Keep the main deployment thread alive so the spawned informed trader threads can run asynchronously.
    std::cout << "All informed traders deployed and running. Press Ctrl+C to terminate." << '\n';

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}
