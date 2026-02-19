#pragma once

#include <string>

namespace gateway {
struct GatewayConfig {
    std::string downstream_order_manager_host;
    int downstream_order_manager_port;
};
} // namespace gateway
