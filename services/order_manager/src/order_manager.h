#ifndef ELSA_FYP_ORDER_MANAGER_H
#define ELSA_FYP_ORDER_MANAGER_H

#include "balance_checker.h"
#include "transport/websocket_server.h"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include <transport/websocket_client.h>

namespace om {
using WebsocketManagerServer = transport::WebsocketManagerServer;
using WebsocketManagerClient = transport::WebsocketManagerClient;

constexpr int ORDER_MANAGER_SERVER_PORT = 6767;
constexpr int GATEWAY_COUNT = 4;

class OrderManager {
  public:
    std::expected<void, std::string> run();

  private:
    std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
        "order_manager_logger", "logs/order_manager.log");

    WebsocketManagerServer inbound_ws_server{ORDER_MANAGER_SERVER_PORT, "localhost", logger};
    WebsocketManagerClient outbound_ws_client{logger};
    BalanceChecker balance_checker{};
};
} // namespace om

#endif // ELSA_FYP_ORDER_MANAGER_H
