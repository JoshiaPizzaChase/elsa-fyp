#ifndef ELSA_FYP_CLIENT_SDK_CONFIG_H
#define ELSA_FYP_CLIENT_SDK_CONFIG_H

#include <string>

namespace mdp {
struct MdpConfig {
    std::string host;
    int ws_port;
};
} // namespace mdp

#endif // ELSA_FYP_CLIENT_SDK_CONFIG_H
