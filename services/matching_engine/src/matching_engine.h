#pragma once

#include "limit_order_book.h"
#include "websocket_server.h"

#include <queue>
#include <string>
#include <vector>

namespace engine {
using WebsocketManagerServer = transport::WebsocketManagerServer;

class MatchingEngine {
  public:
    MatchingEngine(std::string_view host, int port, const std::vector<std::string>& active_symbols,
                   std::chrono::milliseconds flush_interval);
    void init();
    void run();
    void wait_for_connections();

  private:
    WebsocketManagerServer inbound_ws_server;

    OrderbookSnapshotRingBuffer shm_orderbook_snapshot;
    std::chrono::milliseconds flush_interval;

    int incoming_request_connection_id;
    int order_response_connection_id;

    std::queue<Trade> trade_events;

    std::unordered_map<std::string, LimitOrderBook> limit_order_books;
};

} // namespace engine
