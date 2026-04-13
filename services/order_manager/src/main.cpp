#include "configuration/order_manager_config.h"
#include "database_client_wrapper.h"
#include "order_manager.h"
#include "rfl/toml/load.hpp"
#include "transport/inbound_websocket_server.h"
#include "transport/outbound_websocket_client.h"

using namespace om;

int main(int argc, char* argv[]) {
    auto oms_cfg = argc < 2 ? "oms.toml" : argv[1];
    OrderManagerConfig order_manager_config = rfl::toml::load<OrderManagerConfig>(oms_cfg).value();

    const OrderManagerDependencyFactory dependency_factory{
        .create_inbound_server =
            [](std::string_view host, int port, std::shared_ptr<spdlog::logger> logger,
               std::vector<int>& gateway_connection_ids) {
                const auto on_connection_callback =
                    [&](transport::WebsocketManagerServer::ConnectionMetadata::conn_meta_shared_ptr
                            connection_metadata) {
                        gateway_connection_ids.emplace_back(connection_metadata->get_id());
                    };

                return std::make_unique<transport::InboundWebsocketServer>(host, port, logger, true,
                                                                           on_connection_callback);
            },
        .create_outbound_client =
            [](std::shared_ptr<spdlog::logger> logger) {
                return std::make_unique<transport::OutboundWebsocketClient>(logger);
            },
        .create_database_client =
            [](bool ensure_init) { return std::make_unique<DatabaseClientWrapper>(ensure_init); }};

    OrderManager order_manager{order_manager_config.order_manager_host,
                               order_manager_config.order_manager_port, dependency_factory};

    order_manager.init();
    order_manager.connect_matching_engine(order_manager_config.downstream_matching_engine_host,
                                          order_manager_config.downstream_matching_engine_port);

    order_manager.start();

    return 0;
}
