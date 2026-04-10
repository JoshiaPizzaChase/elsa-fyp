#include "configuration/matching_engine_config.h"
#include "matching_engine.h"
#include "rfl/toml/load.hpp"
#include "transport/inbound_websocket_server.h"

using namespace engine;

int main(int argc, char* argv[]) {
    auto me_cfg = argc < 2 ? "me.toml" : argv[1];
    MatchingEngineConfig matching_engine_config =
        rfl::toml::load<MatchingEngineConfig>(me_cfg).value();

    const MatchingEngineDependencyFactory dependency_factory{
        .create_trade_publisher =
            [](std::string_view symbol) {
                return std::make_unique<SharedMemoryPublisher<Trade, TradeRingBuffer>>(
                    TradeRingBuffer ::open_exist_shm(std::format(
                        "{}_{}_{}", symbol, core::constants::TRADE_SHM_FILE, SERVER_NAME)));
            },
        .create_orderbook_snapshot_publisher =
            [](std::string_view symbol) {
                return std::make_unique<SharedMemoryPublisher<TopOrderBookLevelAggregates,
                                                              OrderbookSnapshotRingBuffer>>(
                    OrderbookSnapshotRingBuffer::open_exist_shm(
                        std::format("{}_{}_{}", symbol,
                                    core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE, SERVER_NAME)));
            },

        .create_inbound_server =
            [](std::string_view host, int port, std::shared_ptr<spdlog::logger> logger,
               int& incoming_request_connection_id, int& order_response_connection_id) {
                auto on_connection_callback =
                    [&, logger](WebsocketManagerServer::ConnectionMetadata::conn_meta_shared_ptr
                                    connection_metadata) {
                        const auto counter_party = connection_metadata->get_counter_party();
                        if (counter_party == "order_request") {
                            incoming_request_connection_id = connection_metadata->get_id();
                            logger->info("[ME] Order request connection established, id: {}",
                                         incoming_request_connection_id);
                        } else if (counter_party == "order_response") {
                            order_response_connection_id = connection_metadata->get_id();
                            logger->info("[ME] Order response connection established, id: {}",
                                         order_response_connection_id);
                        } else {
                            logger->error("[ME] Unexpected connection: {}", counter_party);
                        }
                        logger->flush();
                    };

                return std::make_unique<transport::InboundWebsocketServer>(host, port, logger, true,
                                                                           on_connection_callback);
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
