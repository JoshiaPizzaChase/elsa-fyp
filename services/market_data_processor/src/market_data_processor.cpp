#include "market_data_processor.h"

namespace mdp {
MarketDataProcessor::MarketDataProcessor(const MdpConfig& config)
    : websocket_server(config.ws_port, config.host, logger) {
    orderbook_snapshot_ring_buffers.reserve(config.active_symbols.size());
    trade_ring_buffers.reserve(config.active_symbols.size());
    for (const auto& symbol : config.active_symbols) {
        orderbook_snapshot_ring_buffers.emplace_back(
            OrderbookSnapshotRingBuffer::create(std::format(
                "{}_{}_{}", symbol, core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE, SERVER_NAME)));
        trade_ring_buffers.emplace_back(TradeRingBuffer::create(
            std::format("{}_{}_{}", symbol, core::constants::TRADE_SHM_FILE, SERVER_NAME)));
    }
}

[[noreturn]] void MarketDataProcessor::start() {
    if (!websocket_server.start().has_value()) {
        throw std::runtime_error("websocket server starts failed");
    }
    logger->info("MDP started");

    while (true) {
        // publish orderbook snapshot
        for (auto& orderbook_snapshot_ring_buffer : orderbook_snapshot_ring_buffers) {
            auto orderbook_snapshot_res = orderbook_snapshot_ring_buffer.try_pop();
            if (orderbook_snapshot_res.has_value()) {
                orderbook_snapshot_res.value().to_json(orderbook_snapshot_json);
                auto res = websocket_server.send_to_all(orderbook_snapshot_json.dump(),
                                                        transport::MessageFormat::text);
                logger->info(orderbook_snapshot_json.dump());
                logger->info("Orderbook snapshot sent");

                if (!res.has_value()) {
                    auto failed_ids = res.error();
                    std::string error_msg =
                        "MDP failed to publish orderbook snapshot to client id=";
                    for (const auto id : failed_ids) {
                        error_msg += std::to_string(id);
                        if (id != failed_ids.back()) {
                            error_msg += ",";
                        }
                    }
                    logger->error(error_msg);
                }
            }
        }
        // publish public trades
        for (auto& trade_ring_buffer : trade_ring_buffers) {
            auto trade_res = trade_ring_buffer.try_pop();
            if (trade_res.has_value()) {
                trade_res.value().to_json(trade_json);
                auto res =
                    websocket_server.send_to_all(trade_json.dump(), transport::MessageFormat::text);
                if (!res.has_value()) {
                    auto failed_ids = res.error();
                    std::string error_msg = "MDP failed to publish trade to client id=";
                    for (const auto id : failed_ids) {
                        error_msg += std::to_string(id);
                        if (id != failed_ids.back()) {
                            error_msg += ",";
                        }
                    }
                    logger->error(error_msg);
                }
            }
        }
    }
}
} // namespace mdp