#include "configuration/order_manager_config.h"
#include "order_manager.h"
#include "rfl/toml/load.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println("argc: {}", argc);
        std::println("Please pass in program name as arg0");
        std::println("Please pass in Order Manager config path as arg1");
        return -1;
    }

    // Read Order Manager config
    std::cout << "Reading Order Manager config file at: " << argv[1] << std::endl;
    om::OrderManagerConfig order_manager_config =
        rfl::toml::load<om::OrderManagerConfig>(argv[1]).value();

    om::OrderManager order_manager{order_manager_config.order_manager_host,
                                   order_manager_config.order_manager_port,
                                   order_manager_config.gateway_count};

    order_manager.connect_matching_engine(order_manager_config.downstream_matching_engine_host,
                                          order_manager_config.downstream_matching_engine_port);

    order_manager.start();

    return 0;
}
