#pragma once

#include "limit_order_book.h"
#include "shared_memory_publisher.h"
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

    std::unordered_map<std::string, SharedMemoryPublisher<TopOrderBookLevelAggregates,
                                                          OrderbookSnapshotRingBuffer>>
        orderbook_snapshot_publishers; // One publisher for each symbol
    std::chrono::milliseconds flush_interval;

    int incoming_request_connection_id;
    int order_response_connection_id;

    std::queue<Trade> trade_events; // Container for limit order books to dump trade events

    std::unordered_map<std::string, LimitOrderBook>
        limit_order_books; // One limit order book for each symbol
};

} // namespace engine
