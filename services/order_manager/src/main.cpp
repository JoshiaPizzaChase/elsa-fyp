#include "configuration/order_manager_config.h"
#include "order_manager.h"
#include "rfl/toml/load.hpp"
#include "transport/inbound_websocket_server.h"

using namespace om;

int main(int argc, char* argv[]) {
    auto oms_cfg = argc < 2 ? "oms.toml" : argv[1];
    OrderManagerConfig order_manager_config = rfl::toml::load<OrderManagerConfig>(oms_cfg).value();

    const OrderManagerDependencyFactory dependency_factory{
        .create_inbound_server = [](std::string_view host, int port,
                                    std::shared_ptr<spdlog::logger> logger) {
            return std::make_unique<transport::InboundWebsocketServer>(host, port, logger);
        }};

    OrderManager order_manager{order_manager_config.order_manager_host,
                               order_manager_config.order_manager_port,
                               order_manager_config.gateway_count, dependency_factory};

    order_manager.connect_matching_engine(order_manager_config.downstream_matching_engine_host,
                                          order_manager_config.downstream_matching_engine_port);

    order_manager.start();

    return 0;
}
