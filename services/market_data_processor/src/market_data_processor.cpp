#include "market_data_processor.h"

MarketDataProcessor::MarketDataProcessor(int ws_port)
    : orderbook_snapshot_ring_buffer(
          OrderbookSnapshotRingBuffer::create(ORDERBOOK_SNAPSHOT_SHM_FILE)),
      websocket_server(transport::WebsocketManagerServer{ws_port, "localhost"}) {
}

[[noreturn]] void MarketDataProcessor::start() {
    if (!websocket_server.start().has_value()) {
        throw std::runtime_error("websocket server starts failed");
    }
    logger->info("MDP started");
    while (true) {
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
    }
}