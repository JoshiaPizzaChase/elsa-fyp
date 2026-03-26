#pragma once

#include <string>
#include <vector>

namespace engine {
struct MatchingEngineConfig {
    std::string matching_engine_host;
    int matching_engine_port;
    std::vector<std::string> active_symbols;
};
} // namespace engine
