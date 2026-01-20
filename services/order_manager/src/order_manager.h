#ifndef ELSA_FYP_ORDER_MANAGER_H
#define ELSA_FYP_ORDER_MANAGER_H

#include "balance_checker.h"
#include "transport/websocket_server.h"

using WebsocketManagerServer = transport::WebsocketManagerServer;

class OrderManager {
  private:
    WebsocketManagerServer ws_server = WebsocketManagerServer(67);
    BalanceChecker balance_checker = BalanceChecker();
};

#endif // ELSA_FYP_ORDER_MANAGER_H
