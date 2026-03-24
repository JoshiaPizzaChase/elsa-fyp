#pragma once

#include <string>

namespace mdp {
struct MdpConfig {
    std::string host;
    int ws_port;
    std::string server_name; // used for unique shm file name to prevent naming conflict between servers on the same machine 
};
} // namespace mdp
