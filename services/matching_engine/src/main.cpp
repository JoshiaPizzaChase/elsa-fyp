#include "configuration/matching_engine_config.h"
#include "core/orderbook_snapshot.h"
#include "matching_engine.h"
#include "rfl/toml/load.hpp"
#include <iostream>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println("argc: {}", argc);
        std::println("Please pass in program name as arg0");
        std::println("Please pass in Matching Engine config path as arg1");
        return -1;
    }

    // Read Order Manager config
    std::cout << "Reading Matching Engine config file at: " << argv[1] << std::endl;
    engine::MatchingEngineConfig matching_engine_config =
        rfl::toml::load<engine::MatchingEngineConfig>(argv[1]).value();

    engine::MatchingEngine matching_engine{matching_engine_config.matching_engine_host,
                                           matching_engine_config.matching_engine_port};

    matching_engine.start();

    // auto ring_buffer =
    //     OrderbookSnapshotRingBuffer::open_exist_shm(core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE);
    // auto snapshot = TopOrderBookLevelAggregates{"GME"};
    //
    // for (int i = 0; i < 1024; i++) {
    //     int j = i % 50;
    //     snapshot.bid_level_aggregates[j].price = i;
    //     snapshot.bid_level_aggregates[j].quantity = i;
    //     snapshot.ask_level_aggregates[j].price = i + 1;
    //     snapshot.ask_level_aggregates[j].quantity = i + 1;
    //     if (!ring_buffer.try_push(snapshot)) {
    //         std::cerr << "Failed to push snapshot " << "\n";
    //     }
    // }
}
