#include "configuration/matching_engine_config.h"
#include "inbound_websocket_server.h"
#include "matching_engine.h"
#include "rfl/toml/load.hpp"
#include <iostream>
#include <print>

using namespace engine;

int main(int argc, char* argv[]) {
    auto me_cfg = argc < 2 ? "me.toml" : argv[1];
    MatchingEngineConfig matching_engine_config =
        rfl::toml::load<MatchingEngineConfig>(me_cfg).value();

    const MatchingEngineDependencyFactory dependency_factory{
        .create_trade_publisher =
            [](std::string_view symbol) {
                return std::make_unique<SharedMemoryPublisher<Trade, TradeRingBuffer>>(
                    TradeRingBuffer ::open_exist_shm(static_cast<std::string>(symbol) +
                                                     core::constants::TRADE_SHM_FILE + "_" +
                                                     SERVER_NAME));
            },
        .create_orderbook_snapshot_publisher =
            [](std::string_view symbol) {
                return std::make_unique<SharedMemoryPublisher<TopOrderBookLevelAggregates,
                                                              OrderbookSnapshotRingBuffer>>(
                    OrderbookSnapshotRingBuffer::open_exist_shm(
                        static_cast<std::string>(symbol) +
                        core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE + "_" + SERVER_NAME));
            },

        .create_inbound_server =
            [](std::string_view host, int port, std::shared_ptr<spdlog::logger> logger) {
                return std::make_unique<InboundWebsocketServer>(host, port, logger);
            }};

    MatchingEngine matching_engine{
        matching_engine_config.matching_engine_host, matching_engine_config.matching_engine_port,
        matching_engine_config.active_symbols,
        std::chrono::milliseconds{matching_engine_config.snapshot_flush_interval},
        dependency_factory};

    matching_engine.init();
    matching_engine.wait_for_connections();
    matching_engine.run();
}
