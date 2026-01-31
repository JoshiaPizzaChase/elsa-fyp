#ifndef ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
#define ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H

#include "core/orderbook_snapshot.h"
#include "nlohmann/json.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "transport/websocket_server.h"

using json = nlohmann::json;

class MarketDataProcessor {
  private:
    OrderbookSnapshotRingBuffer orderbook_snapshot_ring_buffer;
    transport::WebsocketManagerServer websocket_server;
    std::shared_ptr<spdlog::logger> logger =
        spdlog::basic_logger_mt<spdlog::async_factory>("mdp_logger", "logs/mdp.log");
    json orderbook_snapshot_json;

  public:
    MarketDataProcessor(int ws_port);

    [[noreturn]] void start();
};

#endif // ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
