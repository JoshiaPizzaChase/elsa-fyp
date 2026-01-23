#ifndef ELSA_FYP_ORDER_MANAGER_H
#define ELSA_FYP_ORDER_MANAGER_H

#include "balance_checker.h"
#include "transport/websocket_server.h"

namespace om {
using WebsocketManagerServer = transport::WebsocketManagerServer;

constexpr int ORDER_MANAGER_SERVER_PORT = 67;

class OrderManager {
  private:
    WebsocketManagerServer ws_server = WebsocketManagerServer(ORDER_MANAGER_SERVER_PORT);
    BalanceChecker balance_checker = BalanceChecker();
};
} // namespace om

#endif // ELSA_FYP_ORDER_MANAGER_H
