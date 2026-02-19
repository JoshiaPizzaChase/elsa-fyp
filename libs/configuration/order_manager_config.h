#pragma once

#include <string>

namespace om {
struct OrderManagerConfig {
    std::string order_manager_host;
    int order_manager_port;

    std::string downstream_matching_engine_host;
    int downstream_matching_engine_port;

    int gateway_count;
};
} // namespace om
