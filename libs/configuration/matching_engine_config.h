#pragma once

#include <string>
#include <vector>

namespace engine {
struct MatchingEngineConfig {
    std::string matching_engine_host;
    int matching_engine_port;
    std::string server_name; // used for unique shm file name to prevent naming conflict between servers on the same machine
    std::vector<std::string> active_symbols;
};
} // namespace engine
