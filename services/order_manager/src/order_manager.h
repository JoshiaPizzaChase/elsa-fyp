#pragma once

#include "balance_checker.h"
#include "websocket_server.h"

#include "spdlog/spdlog.h"

#include "core/containers.h"
#include "websocket_client.h"

namespace om {
inline constexpr std::string USD_SYMBOL = "USD";

using WebsocketManagerServer = transport::WebsocketManagerServer;
using WebsocketManagerClient = transport::WebsocketManagerClient;

class OrderManager {
  public:
    OrderManager(std::string_view host, int port, int gateway_count);
    std::expected<void, std::string> connect_matching_engine(std::string host, int port);
    std::expected<void, std::string> start();

  private:
    WebsocketManagerServer inbound_ws_server;
    WebsocketManagerClient order_request_ws_client;
    WebsocketManagerClient order_response_ws_client;
    BalanceChecker balance_checker;

    int gateway_count;
    int order_request_connection_id;
    int order_response_connection_id;

    int current_order_id;
    std::unordered_map<int, int> order_id_map;
};

bool validate_container(const core::Container& container, BalanceChecker& balance_checker,
                        std::optional<int> fill_cost = std::nullopt);
} // namespace om
