#include "order_manager.h"
#include "transport/messaging.h"

namespace om {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

OrderManager::OrderManager(std::string_view host, int port, int gateway_count)
    : logger{spdlog::basic_logger_mt<spdlog::async_factory>(
          "order_manager_logger", std::string{PROJECT_SOURCE_DIR} + "/logs/order_manager.log")},
      inbound_ws_server{port, host, logger}, outbound_ws_client{logger},
      gateway_count{gateway_count}, matching_engine_connection_id{} {
    // TODO: Handle start error
    inbound_ws_server.start();
    outbound_ws_client.start();

    logger->info("Order Manager constructed");
    logger->flush();
}

std::expected<void, std::string> OrderManager::connect_matching_engine(std::string host, int port) {
    // TODO: Handle expected
    auto matching_engine_connection_result =
        outbound_ws_client.connect(std::format("ws://{}:{}", host, port));

    if (matching_engine_connection_result.has_value()) {
        matching_engine_connection_id = matching_engine_connection_result.value();
        logger->info("Matching engine connected");
        logger->flush();
    } else {
        logger->error("Failed to connect to matching engine");
        logger->flush();
    }

    return {};
}

std::expected<void, std::string> OrderManager::start() {
    int curr_gateway_id = 0;

    while (true) {
        if (auto new_message = inbound_ws_server.dequeue_message(curr_gateway_id);
            new_message.has_value()) {
            const auto container = transport::deserialize_container(new_message.value());

            auto new_order_handler{[this](const core::NewOrderSingleContainer& new_order) {
                // TODO: Balance checking
                // TODO: Handle send error
                outbound_ws_client.send(matching_engine_connection_id,
                                        transport::serialize_container(new_order));
                logger->info("New order request received");
                logger->info(transport::serialize_container(new_order));
                logger->flush();
            }};
            auto cancel_order_handler{
                [this](const core::CancelOrderRequestContainer& cancel_request) {
                    // TODO: Handle send error
                    outbound_ws_client.send(matching_engine_connection_id,
                                            transport::serialize_container(cancel_request));
                    logger->info("Cancel order request received");
                    logger->info(transport::serialize_container(cancel_request));
                    logger->flush();
                }};
            auto execution_report_handler{
                [this](const core::ExecutionReportContainer& execution_report) {
                    logger->info("Execution report should not be received by Order Manager");
                    logger->flush();
                }};

            auto catch_all_handler{[this](auto&&) {
                logger->error("Received unexpected request from Gateway");
                logger->flush();
            }};

            std::visit(overloaded{new_order_handler, cancel_order_handler, catch_all_handler},
                       container);
        }

        curr_gateway_id = (curr_gateway_id + 1) % gateway_count;
    }

    return {};
}
} // namespace om
