#include "configuration/mdp_config.h"
#include "market_data_processor.h"
#include "rfl/toml/load.hpp"
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::print("Usage: market_data_processor <toml config path>");
        return -1;
    }
    try {
        mdp::MdpConfig mdp_config = rfl::toml::load<mdp::MdpConfig>(argv[1]).value();
        auto mdp = mdp::MarketDataProcessor(mdp_config);
        mdp.start();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}
