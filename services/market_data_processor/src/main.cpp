#include "configuration/mdp_config.h"
#include "market_data_processor.h"
#include "rfl/toml/load.hpp"
#include <print>

int main(int argc, char* argv[]) {
    try {
        // Read Market Data Processor config
        const auto mdp_cfg = argc == 2 ? argv[1] : "mdp.toml";
        const mdp::MdpConfig mdp_config = rfl::toml::load<mdp::MdpConfig>(mdp_cfg).value();
        auto mdp = mdp::MarketDataProcessor(mdp_config);
        mdp.start();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}
