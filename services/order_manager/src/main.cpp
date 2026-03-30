#include "configuration/order_manager_config.h"
#include "order_manager.h"
#include "rfl/toml/load.hpp"
#include <print>

int main(int argc, char* argv[]) {
    auto oms_cfg = argc < 2 ? "oms.toml" : argv[1];
    om::OrderManagerConfig order_manager_config =
        rfl::toml::load<om::OrderManagerConfig>(oms_cfg).value();

    om::OrderManager order_manager{order_manager_config.order_manager_host,
                                   order_manager_config.order_manager_port,
                                   order_manager_config.gateway_count};

    order_manager.connect_matching_engine(order_manager_config.downstream_matching_engine_host,
                                          order_manager_config.downstream_matching_engine_port);

    order_manager.start();

    return 0;
}
