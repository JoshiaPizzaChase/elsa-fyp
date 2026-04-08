#pragma once

#include "core/containers.h"
#include "limit_order_book.h"
#include "shared_memory_publisher.h"
#include "transport/inbound_server.h"
#include "websocket_server.h"

#include <queue>
#include <string>
#include <vector>

namespace engine {
using WebsocketManagerServer = transport::WebsocketManagerServer;

struct MatchingEngineDependencyFactory {
    std::function<std::unique_ptr<Publisher<Trade>>(std::string_view)> create_trade_publisher;

    std::function<std::unique_ptr<Publisher<TopOrderBookLevelAggregates>>(std::string_view)>
        create_orderbook_snapshot_publisher;

    std::function<std::unique_ptr<transport::InboundServer>(std::string_view, int,
                                                 std::shared_ptr<spdlog::logger>)>
        create_inbound_server;
};

class MatchingEngine {
  public:
    MatchingEngine(std::string_view host, int port, const std::vector<std::string>& active_symbols,
                   std::chrono::milliseconds flush_interval,
                   const MatchingEngineDependencyFactory& dependency_factory);
    void init() const;
    [[noreturn]] void run();
    void wait_for_connections();

    [[nodiscard]] const std::unordered_map<std::string, LimitOrderBook>&
    get_limit_order_books() const;
    [[nodiscard]] const std::unordered_map<std::string,
                                           std::unique_ptr<Publisher<TopOrderBookLevelAggregates>>>&
    get_snapshot_publishers() const;

  private:
    std::unique_ptr<transport::InboundServer> inbound_server;

    std::unordered_map<std::string,
                       std::unique_ptr<Publisher<TopOrderBookLevelAggregates>>>
        orderbook_snapshot_publishers; // One publisher for each symbol
    std::chrono::milliseconds flush_interval;

    int incoming_request_connection_id;
    int order_response_connection_id;

    std::queue<Trade> trade_events; // Container for limit order books to dump trade events

    std::unordered_map<std::string, LimitOrderBook>
        limit_order_books; // One limit order book for each symbol
};

void process_container(const core::Container& container,
                       std::unordered_map<std::string, LimitOrderBook>& limit_order_books,
                       std::queue<Trade>& trade_events, transport::InboundServer& inbound_server,
                       int order_response_connection_id, int incoming_request_connection_id);

} // namespace engine
