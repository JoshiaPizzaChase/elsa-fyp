#pragma once

#include "configuration/mdp_config.h"
#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "nlohmann/json.hpp"
#include "logger/logger.h"
#include "websocket_server.h"
#include <vector>

using json = nlohmann::json;

namespace mdp {
class MarketDataProcessor {
  private:
    std::shared_ptr<spdlog::logger> logger = logger::create_logger(
        "mdp_logger",
        std::format("{}/logs/{}/mdp.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME));
    std::vector<OrderbookSnapshotRingBuffer> orderbook_snapshot_ring_buffers;
    std::vector<TradeRingBuffer> trade_ring_buffers;
    transport::WebsocketManagerServer websocket_server;
    json orderbook_snapshot_json;
    json trade_json;

  public:
    MarketDataProcessor(const MdpConfig& config);

    [[noreturn]] void start();
};
} // namespace mdp
