#pragma once

#include <vector>
#include <string>

namespace mdp {
struct MdpConfig {
    std::string host;
    int ws_port;
    std::vector<std::string> active_symbols; // should be aligned with upstream matching engine
};
} // namespace mdp
