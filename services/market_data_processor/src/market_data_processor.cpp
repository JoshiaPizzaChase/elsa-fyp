#include "market_data_processor.h"

namespace mdp {
MarketDataProcessor::MarketDataProcessor(MdpConfig config)
    : orderbook_snapshot_ring_buffer(
          OrderbookSnapshotRingBuffer::create(core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE)),
      trade_ring_buffer(TradeRingBuffer::create(core::constants::TRADE_SHM_FILE)),
      websocket_server(transport::WebsocketManagerServer{config.ws_port, config.host}) {
}

[[noreturn]] void MarketDataProcessor::start() {
    if (!websocket_server.start().has_value()) {
        throw std::runtime_error("websocket server starts failed");
    }
    logger->info("MDP started");
    while (true) {
        // publish orderbook snapshot
        auto orderbook_snapshot_res = orderbook_snapshot_ring_buffer.try_pop();
        if (orderbook_snapshot_res.has_value()) {
            orderbook_snapshot_res.value().to_json(orderbook_snapshot_json);
            auto res = websocket_server.send_to_all(orderbook_snapshot_json.dump());
            if (!res.has_value()) {
                auto failed_ids = res.error();
                std::string error_msg = "MDP failed to publish orderbook snapshot to client id=";
                for (const auto id : failed_ids) {
                    error_msg += std::to_string(id);
                    if (id != failed_ids.back()) {
                        error_msg += ",";
                    }
                }
                logger->error(error_msg);
            }
        }
        // publish public trades
        auto trade_res = trade_ring_buffer.try_pop();
        if (trade_res.has_value()) {
            trade_res.value().to_json(trade_json);
            auto res = websocket_server.send_to_all(trade_json.dump());
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
} // namespace mdp