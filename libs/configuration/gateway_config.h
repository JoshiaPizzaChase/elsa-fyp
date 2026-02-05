#ifndef ELSA_FYP_GATEWAY_CONFIG_H
#define ELSA_FYP_GATEWAY_CONFIG_H

#include <string>

namespace gateway {
struct GatewayConfig {
    std::string downstream_order_manager_host;
    int downstream_order_manager_port;
};
} // namespace gateway

#endif // ELSA_FYP_GATEWAY_CONFIG_H
