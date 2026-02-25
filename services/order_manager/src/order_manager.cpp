#include "order_manager.h"
#include "transport/messaging.h"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace om {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

static std::shared_ptr<spdlog::logger> logger{spdlog::basic_logger_mt<spdlog::async_factory>(
    "order_manager_logger", std::string{PROJECT_SOURCE_DIR} + "/logs/order_manager.log")};

OrderManager::OrderManager(std::string_view host, int port, int gateway_count)
    : inbound_ws_server{port, host, logger}, outbound_ws_client{logger},
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

            if (validate_container(container, balance_checker)) {
                // TODO: Handle send error
                outbound_ws_client.send(matching_engine_connection_id, new_message.value());
                logger->info(new_message.value());
                logger->flush();
            }
        }

        curr_gateway_id = (curr_gateway_id + 1) % gateway_count;
    }

    return {};
}

bool validate_container(const core::Container& container, BalanceChecker& balance_checker) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        switch (new_order.side) {
        case core::Side::bid:
            switch (new_order.ord_type) {
            case core::OrderType::limit:
                if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id, USD_SYMBOL,
                                                            -new_order.price.value() *
                                                                new_order.order_qty)) {
                    return false;
                }

                balance_checker.update_balance(new_order.sender_comp_id, USD_SYMBOL,
                                               -new_order.price.value() * new_order.order_qty);
                break;
            case core::OrderType::market:
                // Fetch fill cost from Matching Engine
                // 1. Thru REST API???
                // 2. ME has not fully processed its order queue -> Returned cost is
                // inaccurate?

                // Compare
                return false;
                break;
            default:
                assert(false && "Unsupported Order Type");
            }
            break;
        case core::Side::ask:
            if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id, new_order.symbol,
                                                        -new_order.order_qty)) {
                return false;
            }

            balance_checker.update_balance(new_order.sender_comp_id, new_order.symbol,
                                           -new_order.order_qty);
            break;
        default:
            assert(false && "UNREACHABLE");
        }

        return true;
    }};
    auto cancel_order_handler{[](const core::CancelOrderRequestContainer& _) { return true; }};
    auto catch_all_handler{[](auto&& _) {
        logger->error("Received unexpected request from Gateway");
        logger->flush();
        return false;
    }};

    return std::visit(overloaded{new_order_handler, cancel_order_handler, catch_all_handler},
                      container);
}
} // namespace om
