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
    WebsocketManagerClient outbound_ws_client;
    BalanceChecker balance_checker;

    int gateway_count;
    int matching_engine_connection_id;
};

bool validate_container(const core::Container& container, BalanceChecker& balance_checker,
                        std::optional<int> fill_cost = std::nullopt);
} // namespace om
