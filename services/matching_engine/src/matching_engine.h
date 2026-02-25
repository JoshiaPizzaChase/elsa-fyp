#pragma once

#include "limit_order_book.h"
#include "websocket_server.h"
#include <string>

namespace engine {
using WebsocketManagerServer = transport::WebsocketManagerServer;
class MatchingEngine {
  public:
    MatchingEngine(std::string_view host, int port);
    std::expected<void, std::string> start();

  private:
    std::shared_ptr<spdlog::logger> logger;

    WebsocketManagerServer inbound_ws_server;

    OrderbookSnapshotRingBuffer ring_buffer;

    // TODO: Support multiple tickers
    LimitOrderBook limit_order_book;
    // TODO: Order ID generation
    int latest_order_id;
};

} // namespace engine
