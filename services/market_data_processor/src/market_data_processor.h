#ifndef ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
#define ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H

#include "configuration/mdp_config.h"
#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "nlohmann/json.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "websocket_server.h"

using json = nlohmann::json;

namespace mdp {
class MarketDataProcessor {
  private:
    std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
        "mdp_logger", std::string(PROJECT_ROOT_DIR) + "/logs/mdp.log");
    OrderbookSnapshotRingBuffer orderbook_snapshot_ring_buffer;
    TradeRingBuffer trade_ring_buffer;
    transport::WebsocketManagerServer websocket_server;
    json orderbook_snapshot_json;
    json trade_json;

  public:
    MarketDataProcessor(MdpConfig config);

    [[noreturn]] void start();
};
} // namespace mdp

#endif // ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
