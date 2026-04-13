#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <chrono>

#include "../traders/oracle_service.h"
#include "simulation_config.h"
#include "rfl/toml/load.hpp"
#include <transport/websocket/websocket_server.h>

using namespace simulation;

int main() {
    const auto oracle_config_path = "oracle_config.toml";
    
    simulation::OracleServerConfig config;
    auto toml_result = rfl::toml::load<simulation::OracleServerConfig>(oracle_config_path);
    if (toml_result) {
        config = toml_result.value();
        std::cout << "Loaded Oracle config from " << oracle_config_path << '\n';
    } else {
        std::cerr << "Warning: Could not load " << oracle_config_path << ", using default Oracle configuration.\n";
    }

    std::cout << "Deploying Fundamental Price Oracle..." << '\n';

    // Create Websocket Server for Oracle broadcast
    // Typically, we want this on a separate port from the main exchange data
    int port = 9005; 
    auto ws_server = std::make_shared<transport::WebsocketManagerServer>(port, "0.0.0.0", "oracle_ws_logger", true);
    
    // Start the Websocket Server
    auto start_res = ws_server->start();
    if (!start_res.has_value()) {
        std::cerr << "Failed to start Oracle websocket server on port " << port << '\n';
        return 1;
    }

    // Set up initial true prices for tickers
    std::map<std::string, double> initial_prices;
    for (const auto& ticker : config.tickers) {
        initial_prices[ticker] = 100.0; // Default seed price, could also be config-driven
    }

    // Map OracleServerConfig to OracleConfig for the service
    OracleConfig svc_config;
    svc_config.mu = config.mu;
    svc_config.sigma = config.sigma;
    svc_config.jump_intensity = config.jump_intensity;
    svc_config.jump_mean = config.jump_mean;
    svc_config.jump_std = config.jump_std;
    svc_config.update_interval_ms = config.update_interval_ms;

    // Instantiate and start OracleService
    auto oracle_service = std::make_shared<OracleService>(
        config.tickers,
        initial_prices,
        ws_server,
        svc_config
    );

    oracle_service->start();

    std::cout << "Oracle deployed and broadcasting fundamental prices on port " << port << ".\n";
    std::cout << "Press Ctrl+C to terminate." << '\n';

    // Keep the main deployment thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}