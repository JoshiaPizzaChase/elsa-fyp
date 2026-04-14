#pragma once

#include <string>
#include <vector>

namespace om {
struct OrderManagerConfig {
    std::string order_manager_host;
    int order_manager_port;
    std::vector<std::string> active_symbols;

    std::string downstream_matching_engine_host;
    int downstream_matching_engine_port;
};
} // namespace om
