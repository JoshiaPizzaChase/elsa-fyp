#pragma once

#include "configuration/matching_engine_config.h"
#include "limit_order_book.h"
#include "websocket_server.h"

#include <string>
#include <vector>

namespace engine {
using WebsocketManagerServer = transport::WebsocketManagerServer;
class MatchingEngine {
  public:
    MatchingEngine(std::string_view host, int port, const std::vector<std::string>& active_symbols,
                   std::chrono::milliseconds flush_interval);
    std::expected<void, std::string> start();
    void wait_for_connections();

  private:
    std::shared_ptr<spdlog::logger> logger;

    WebsocketManagerServer inbound_ws_server;

    OrderbookSnapshotRingBuffer shm_orderbook_snapshot;
    std::chrono::milliseconds flush_interval;

    int incoming_request_connection_id;
    int order_response_connection_id;

    std::queue<Trade> trade_events;

    std::unordered_map<std::string, LimitOrderBook> limit_order_books;
};

} // namespace engine
