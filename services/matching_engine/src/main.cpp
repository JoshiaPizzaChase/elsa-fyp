#include "configuration/matching_engine_config.h"
#include "core/orderbook_snapshot.h"
#include "matching_engine.h"
#include "rfl/toml/load.hpp"
#include <iostream>
#include <print>

int main(int argc, char* argv[]) {
    auto me_cfg = argc < 2 ? "me.toml" : argv[1];
    engine::MatchingEngineConfig matching_engine_config =
        rfl::toml::load<engine::MatchingEngineConfig>(me_cfg).value();

    engine::MatchingEngine matching_engine{matching_engine_config.matching_engine_host,
                                           matching_engine_config.matching_engine_port};

    matching_engine.start();
}
