#pragma once

#include "configuration/mdp_config.h"
#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "nlohmann/json.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "websocket_server.h"
#include <vector>

using json = nlohmann::json;

namespace mdp {
class MarketDataProcessor {
  private:
    std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
        "mdp_logger",std::format("{}/logs/{}/mdp.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME));
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
