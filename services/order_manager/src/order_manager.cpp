#include "order_manager.h"
#include "transport/messaging.h"

namespace om {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

std::expected<void, std::string> OrderManager::run() {
    // TODO: Handle start error
    inbound_ws_server.start();
    outbound_ws_client.start();
    // TODO: Get uri from config
    // TODO: Handle connection error
    auto matching_engine_connection_id = outbound_ws_client.connect("ws://localhost:1234").value();

    int curr_gateway_id = 0;

    while (true) {
        if (auto new_message = inbound_ws_server.dequeue_message(curr_gateway_id);
            new_message.has_value()) {
            auto container = transport::deserialize_container(new_message.value());

            auto new_order_handler{[this, matching_engine_connection_id](
                                       const core::NewOrderSingleContainer& new_order) {
                // TODO: Balance checking
                // TODO: Handle send error
                outbound_ws_client.send(matching_engine_connection_id,
                                        transport::serialize_container(new_order));
            }};
            auto cancel_order_handler{[this, matching_engine_connection_id](
                                          const core::CancelOrderRequestContainer& cancel_request) {
                // TODO: Handle send error
                outbound_ws_client.send(matching_engine_connection_id,
                                        transport::serialize_container(cancel_request));
            }};
            auto catch_all_handler{
                [this](auto&&) { logger->error("Received unexpected request from Gateway"); }};

            std::visit(overloaded{new_order_handler, cancel_order_handler, catch_all_handler},
                       container);
        }

        curr_gateway_id = (curr_gateway_id + 1) % GATEWAY_COUNT;
    }

    return {};
}
} // namespace om
