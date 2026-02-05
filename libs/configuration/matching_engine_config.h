#ifndef ELSA_FYP_MATCHING_ENGINE_CONFIG_H
#define ELSA_FYP_MATCHING_ENGINE_CONFIG_H

#include <string>

namespace engine {
struct MatchingEngineConfig {
    std::string matching_engine_host;
    int matching_engine_port;
};
} // namespace engine

#endif // ELSA_FYP_MATCHING_ENGINE_CONFIG_H
