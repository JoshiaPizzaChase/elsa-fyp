#ifndef ELSA_FYP_ORDER_MANAGER_H
#define ELSA_FYP_ORDER_MANAGER_H

#include "balance_checker.h"
#include "transport/websocket_server.h"

namespace om {
using WebsocketManagerServer = transport::WebsocketManagerServer;

constexpr int ORDER_MANAGER_SERVER_PORT = 67;

class OrderManager {
  public:
    OrderManager();

  private:
    WebsocketManagerServer ws_server{ORDER_MANAGER_SERVER_PORT};
    BalanceChecker balance_checker{};
};
} // namespace om

#endif // ELSA_FYP_ORDER_MANAGER_H
