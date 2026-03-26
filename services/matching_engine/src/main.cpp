#include "configuration/matching_engine_config.h"
#include "matching_engine.h"
#include "rfl/toml/load.hpp"
#include <iostream>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::println("argc: {}", argc);
        std::println("Please pass in program name as arg0");
        std::println("Please pass in Matching Engine config path as arg1");
        std::println("Please pass in list of active symbols as arg2");
        std::println("Please pass in order book snapshot flush interval as arg3");
        return -1;
    }

    // Read Order Manager config
    std::cout << "Reading Matching Engine config file at: " << argv[1] << std::endl;
    engine::MatchingEngineConfig matching_engine_config =
        rfl::toml::load<engine::MatchingEngineConfig>(argv[1]).value();

    engine::MatchingEngine matching_engine{
        matching_engine_config.matching_engine_host, matching_engine_config.matching_engine_port,
        matching_engine_config.active_symbols,
        std::chrono::milliseconds{matching_engine_config.snapshot_flush_interval}};

    matching_engine.wait_for_connections();
    matching_engine.start();
}
