#include "order_manager.h"

namespace om {
OrderManager::OrderManager() {
    ws_server.start();
}
} // namespace om
