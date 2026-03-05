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

    balance_checker.update_balance("CLIENT_1", "USD", 50000000);
    balance_checker.update_balance("CLIENT_1", "GME", 50000);
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

            std::optional<int> market_bid_fill_cost{std::nullopt};
            if (const auto* new_order = std::get_if<core::NewOrderSingleContainer>(&container);
                new_order && new_order->ord_type == core::OrderType::market &&
                new_order->side == core::Side::bid) {
                // Fetch fill cost from Matching Engine
                outbound_ws_client.send(matching_engine_connection_id,
                                        transport::serialize_container(core::FillCostQueryContainer{
                                            .symbol = new_order->symbol,
                                            .quantity = new_order->order_qty,
                                            .side = new_order->side}));

                auto response_message =
                    outbound_ws_client.wait_and_dequeue_message(matching_engine_connection_id);
                while (!response_message.has_value()) {
                    response_message =
                        outbound_ws_client.wait_and_dequeue_message(matching_engine_connection_id);
                }

                logger->info("Fill cost response received from ME!");
                logger->flush();

                const auto response_container =
                    transport::deserialize_container(response_message.value());
                assert(
                    std::holds_alternative<core::FillCostResponseContainer>(response_container) &&
                    "Unexpected container type received from Matching Engine");

                market_bid_fill_cost =
                    std::get<core::FillCostResponseContainer>(response_container).total_cost;
            }

            if (validate_container(container, balance_checker, market_bid_fill_cost)) {
                // TODO: Handle send error
                outbound_ws_client.send(matching_engine_connection_id, new_message.value());
                logger->info(new_message.value());
                logger->flush();
            } else {
                logger->info("Message rejected");
                logger->info(new_message.value());
                logger->flush();
            }
        }

        curr_gateway_id = (curr_gateway_id + 1) % gateway_count;
    }

    return {};
}

bool validate_container(const core::Container& container, BalanceChecker& balance_checker,
                        std::optional<int> market_bid_fill_cost) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        logger->info("GME: {}",
                     balance_checker.get_balance(new_order.sender_comp_id, new_order.symbol));
        logger->info("USD: {}", balance_checker.get_balance(new_order.sender_comp_id, USD_SYMBOL));
        logger->info("Price: {}", new_order.price.value_or(-1));
        logger->info("Quantity: {}", new_order.order_qty);
        logger->info("fill cost: {}", market_bid_fill_cost.value_or(-1));
        logger->flush();

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
                if (market_bid_fill_cost.has_value()) {
                    if (!balance_checker.has_sufficient_balance(
                            new_order.sender_comp_id, USD_SYMBOL, -market_bid_fill_cost.value())) {
                        return false;
                    }
                    balance_checker.update_balance(new_order.sender_comp_id, USD_SYMBOL,
                                                   -market_bid_fill_cost.value());
                } else {
                    // TODO: change to return error
                    return false;
                }
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
