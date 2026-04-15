#include "order_manager.h"
#include "logger/logger.h"
#include "transport/messaging.h"

#include <boost/contract.hpp>
#include <boost/uuid.hpp>
#include <cstdint>

namespace om {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

static std::shared_ptr<spdlog::logger> logger{logger::create_logger(
    "order_manager_logger",
    std::format("{}/logs/{}/order_manager.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME))};

OrderManager::OrderManager(std::string_view host, int port,
                           const std::vector<std::string>& active_symbols,
                           const OrderManagerDependencyFactory& dependency_factory)
    : active_symbols{active_symbols.begin(), active_symbols.end()}, order_request_connection_id{-1},
      order_response_connection_id{-1}, inbound_server{dependency_factory.create_inbound_server(
                                            host, port, logger, gateway_connection_ids)},
      order_request_outbound_client{dependency_factory.create_outbound_client(logger)},
      order_response_outbound_client{dependency_factory.create_outbound_client(logger)},
      database_client{dependency_factory.create_database_client(true)}, server_id{-1} {
}

void OrderManager::init() {
    std::ignore =
        inbound_server->start()
            .transform([] { logger->info("[OM] Order Manager starts accepting connections"); })
            .or_else([](int) -> std::expected<void, int> {
                logger->error("[OM] Failed to start inbound connection server");

                std::terminate();
            });

    std::ignore = order_request_outbound_client->start()
                      .transform([] { logger->info("[OM] Order Request Client started"); })
                      .or_else([](int) -> std::expected<void, int> {
                          logger->error("[OM] Order Request Client failed to start");

                          std::terminate();
                      });

    std::ignore = order_response_outbound_client->start()
                      .transform([] { logger->info("[OM] Order Response Client started"); })
                      .or_else([](int) -> std::expected<void, int> {
                          logger->error("[OM] Order Response Client failed to start");

                          std::terminate();
                      });

    init_balance_checker(balance_checker, username_user_id_map, *database_client);

    database_client->get_server(SERVER_NAME)
        .transform([&](std::optional<DbServerRow>&& server_row_res) {
            std::move(server_row_res)
                .transform([&](DbServerRow&& server_row) {
                    server_id = server_row.server_id;
                    return server_row;
                })
                .or_else([] -> std::optional<DbServerRow> {
                    logger->error("Failed to find Server Info of server {}", SERVER_NAME);
                    std::terminate();
                    return std::nullopt;
                });
        })
        .transform_error([](std::string&& err) {
            logger->error("Error occured while fetching Server Info from DB: {}", err);
            std::terminate();
            return err;
        });
}

// Load all user balances into the balance_checker
void init_balance_checker(BalanceChecker& balance_checker,
                          OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                          OrderManagerDatabase& database_client) {
    boost::contract::check c = boost::contract::function().precondition(
        [] { BOOST_CONTRACT_ASSERT(SERVER_NAME != nullptr); });

    const std::string_view server_name{SERVER_NAME};

    std::ignore =
        database_client.get_all_users_balances_for_server(server_name)
            .transform([&](std::vector<DbUserBalanceInfo>&& users_balances) {
                assert(users_balances.size() != 0 && "No user balances loaded from DB");
                logger->info("[OM] Loading balances for {} users into balance_checker",
                             users_balances.size());

                for (const auto& [user_id, username, balances] : users_balances) {
                    for (const auto& [symbol, balance] : balances) {
                        // Initialize balance in balance_checker (delta of 0 just sets the
                        // balance)
                        balance_checker.update_balance(username, symbol, balance);
                        logger->info("[OM] Loaded balance for user {}: {} {}", username, balance,
                                     symbol);

                        // TODO: Refine this temporary patchwork
                        username_user_id_map.emplace(username, user_id);
                    }
                }
            })
            .transform_error([](std::string&& err) {
                logger->error("[OM] Failed to load balances into balance_checker: {}", err);
                std::terminate();
                return err;
            });
}

// Spin locks until Order Manager is connected from at least one Gateway
void OrderManager::wait_for_connections() const {
    while (gateway_connection_ids.size() == 0) {
        logger->info("Waiting for connections from Gateway");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    logger->info("At least one connection from Gateway have been established");
}

void OrderManager::connect_matching_engine(std::string host, int port, int try_attempts) {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(try_attempts > 0); });

    std::expected<int, int> order_request_res{};
    for (auto i{0}; i < try_attempts; i++) {
        order_request_res = order_request_outbound_client
                                ->connect(std::format("ws://{}:{}", host, port), "order_request")
                                .transform([this](int connection_id) -> int {
                                    order_request_connection_id = connection_id;
                                    logger->info("[OM] Order Request connection established");

                                    return connection_id;
                                })
                                .or_else([=](int err) -> std::expected<int, int> {
                                    logger->error("[OM] Failed to establish Order Request "
                                                  "connection, "
                                                  "attempt: {}",
                                                  i + 1);

                                    return std::unexpected{err};
                                });

        if (order_request_res)
            break;
    }
    assert(order_request_res.has_value() && "Order Request connection failed to establish");

    std::expected<int, int> order_response_res{};
    for (auto i{0}; i < try_attempts; i++) {
        order_response_res = order_response_outbound_client
                                 ->connect(std::format("ws://{}:{}", host, port), "order_response")
                                 .transform([this](int connection_id) -> int {
                                     order_response_connection_id = connection_id;
                                     logger->info("[OM] Order Response connection established");

                                     return connection_id;
                                 })
                                 .or_else([=](int err) -> std::expected<int, int> {
                                     logger->error("[OM] Failed to establish Order Response "
                                                   "connection, "
                                                   "attempt: {}",
                                                   i + 1);

                                     return std::unexpected{err};
                                 });

        if (order_response_res)
            break;
    }
    assert(order_response_res.has_value() && "Order Response connection failed to establish");
}

void OrderManager::run() {
    auto gateway_ids_it = gateway_connection_ids.cbegin();

    while (true) {
        // Process an incoming request
        inbound_server->dequeue_message(*gateway_ids_it).transform([&](std::string&& new_message) {
            auto container = transport::deserialize_container(new_message);

            preprocess_container(container, order_id_map, order_info_map, username_user_id_map,
                                 *gateway_ids_it, *order_request_outbound_client,
                                 order_request_connection_id)
                .transform([&](std::optional<int>&& market_bid_fill_cost) {
                    const std::string validation_result = validate_container(
                        container, active_symbols, balance_checker, market_bid_fill_cost);

                    if (validation_result == "ok") {
                        forward_and_reply(true, container, order_info_map, *gateway_ids_it,
                                          *order_request_outbound_client,
                                          order_request_connection_id, *inbound_server);
                    } else {
                        forward_and_reply(false, container, order_info_map, *gateway_ids_it,
                                          *order_request_outbound_client,
                                          order_request_connection_id, *inbound_server,
                                          validation_result);
                    }

                    update_database(container, server_id, username_user_id_map, balance_checker,
                                    *database_client, validation_result == "ok");
                })
                .transform_error([&](std::string&& err) {
                    forward_and_reply(false, container, order_info_map, *gateway_ids_it,
                                      *order_request_outbound_client, order_request_connection_id,
                                      *inbound_server, err);

                    update_database(container, server_id, username_user_id_map, balance_checker,
                                    *database_client, false);
                    return err;
                });

            return new_message;
        });

        ++gateway_ids_it;
        if (gateway_ids_it == gateway_connection_ids.cend()) {
            gateway_ids_it = gateway_connection_ids.cbegin();
        }

        // Process a response container from matching engine
        order_response_outbound_client->dequeue_message(order_response_connection_id)
            .transform([&](std::string&& new_message) {
                const auto container = transport::deserialize_container(new_message);

                update_internal_data(container, order_info_map, balance_checker);

                return_execution_report(container, order_id_map, order_info_map, *inbound_server);

                update_database(container, server_id, username_user_id_map, balance_checker,
                                *database_client);

                return new_message;
            });
    }
}

std::expected<std::optional<int>, std::string>
preprocess_container(core::Container& container, OrderManager::OrderIdMapContainer& order_id_map,
                     OrderManager::OrderInfoMapContainer& order_info_map,
                     const OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                     int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                     int order_request_connection_id) {
    // TODO: Fetch latest_assigned_order_id from DB
    static int latest_assigned_order_id{-1};
    auto new_order_handler{[&](core::NewOrderSingleContainer& new_order)
                               -> std::expected<std::optional<int>, std::string> {
        // Assign an internal order_id to NewOrderSingleContainer
        new_order.order_id = ++latest_assigned_order_id;
        logger->info("[OM] New Order Single received: {}", new_order);

        if (!username_user_id_map.contains(new_order.sender_comp_id)) {
            return std::unexpected{std::string{"Order request contains unknown username"}};
        }

        const auto transformed_cl_ord_id = new_order.cl_ord_id * core::constants::max_user_count +
                                           username_user_id_map.at(new_order.sender_comp_id);

        if (order_id_map.right.find(transformed_cl_ord_id) == order_id_map.right.end()) {
            order_id_map.insert(
                OrderManager::OrderIdPair(new_order.order_id.value(), transformed_cl_ord_id));
        } else {
            return std::unexpected{std::string{"Client order id has already been used"}};
        }

        order_info_map.emplace(new_order.order_id.value(),
                               OrderInfo{.sender_comp_id = new_order.sender_comp_id,
                                         .symbol = new_order.symbol,
                                         .side = new_order.side,
                                         .price = new_order.price,
                                         .time_in_force = new_order.time_in_force,
                                         .leaves_qty = new_order.order_qty,
                                         .cum_qty = 0,
                                         .avg_px = 0,
                                         .arrival_gateway_id = arrival_gateway_id});

        // Market bid requires fill cost before proceeding
        if (new_order.ord_type == core::OrderType::market && new_order.side == core::Side::bid) {
            // Fetch fill cost from Matching Engine
            const auto fill_cost_query =
                core::FillCostQueryContainer{.symbol = new_order.symbol,
                                             .quantity = new_order.order_qty,
                                             .side = core::Side::ask};
            return order_request_ws_client
                .send(order_request_connection_id, transport::serialize_container(fill_cost_query))
                .transform([&] -> std::optional<int> {
                    logger->info("[OM] Successfully sent Fill Cost Query: {}", fill_cost_query);

                    auto response_message = order_request_ws_client.wait_and_dequeue_message(
                        order_request_connection_id);
                    while (!response_message.has_value()) {
                        response_message = order_request_ws_client.wait_and_dequeue_message(
                            order_request_connection_id);
                    }

                    const auto response_container =
                        transport::deserialize_container(response_message.value());
                    assert(std::holds_alternative<core::FillCostResponseContainer>(
                               response_container) &&
                           "Unexpected container type received from Matching Engine");
                    const auto fill_cost_response =
                        std::get<core::FillCostResponseContainer>(response_container);
                    logger->info("[OM] Fill Cost Response received: {}", fill_cost_response);

                    return fill_cost_response.total_cost;
                })
                .transform_error([&](int) -> std::string {
                    logger->error("[OM] Failed to send Fill Cost Query: {}", fill_cost_query);

                    return std::string{"Order request dropped due to internal reasons"};
                });
        }

        return std::nullopt;
    }};

    auto cancel_request_handler{[&](core::CancelOrderRequestContainer& cancel_request)
                                    -> std::expected<std::optional<int>, std::string> {
        logger->info("[OM] Cancel Order Request received: {}", cancel_request);

        if (!username_user_id_map.contains(cancel_request.sender_comp_id)) {
            return std::unexpected{std::string{"Cancel request contains unknown username"}};
        }

        // If cancel request does not have internal order_id, fill it in
        // But if the cancel request is for a non-existent orig_cl_ord_id, leave order_id blank
        const auto transformed_orig_cl_ord_id =
            cancel_request.orig_cl_ord_id * core::constants::max_user_count +
            username_user_id_map.at(cancel_request.sender_comp_id);

        if (const auto it = order_id_map.right.find(transformed_orig_cl_ord_id);
            it != order_id_map.right.end()) {
            cancel_request.order_id.emplace(it->second);
        }

        return std::nullopt;
    }};

    auto catch_all_handler{[](auto&) -> std::expected<std::optional<int>, std::string> {
        logger->error("[OM] UNREACHABLE");

        std::terminate();
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

std::string validate_container(const core::Container& container,
                               const std::unordered_set<std::string>& active_symbols,
                               BalanceChecker& balance_checker,
                               std::optional<int> market_bid_fill_cost) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) -> std::string {
        logger->info("[OM] Validating New Order Single: {}", new_order);

        if (!active_symbols.contains(new_order.symbol)) {
            return "Unrecognized symbol";
        }

        // A broker must at least have a record for USD at the start
        if (!balance_checker.broker_id_exists(new_order.sender_comp_id)) {
            return "Balance record for not found during validation";
        }

        switch (new_order.side) {
        case core::Side::bid:
            if (!balance_checker.broker_owns_ticker(new_order.sender_comp_id, USD_SYMBOL)) {
                return "User has no USD balance record";
            }

            switch (new_order.ord_type) {
            case core::OrderType::limit: {
                const std::int64_t reserved_usd =
                    -static_cast<std::int64_t>(new_order.price.value()) * new_order.order_qty;
                if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id, USD_SYMBOL,
                                                            reserved_usd)) {
                    return "User has insufficient USD balance";
                }

                balance_checker.update_balance(new_order.sender_comp_id, USD_SYMBOL, reserved_usd);
                break;
            }
            case core::OrderType::market:
                return market_bid_fill_cost
                    .transform([&](int fill_cost) {
                        if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id,
                                                                    USD_SYMBOL, -fill_cost)) {
                            return "User has insufficient USD balance";
                        }

                        balance_checker.update_balance(new_order.sender_comp_id, USD_SYMBOL,
                                                       -fill_cost);

                        return "ok";
                    })
                    .value_or("User has insufficient USD balance");

                break;
            default:
                logger->error("[OM] Unsupported Order Type");

                std::terminate();
            }
            break;
        case core::Side::ask:
            if (!balance_checker.broker_owns_ticker(new_order.sender_comp_id, new_order.symbol)) {
                return std::format("User has no {} record", new_order.symbol);
            }

            if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id, new_order.symbol,
                                                        -new_order.order_qty)) {
                return std::format("User has insufficient {} balance", new_order.symbol);
            }

            balance_checker.update_balance(new_order.sender_comp_id, new_order.symbol,
                                           -new_order.order_qty);

            break;
        default:
            logger->error("[OM] Unreachable");

            std::terminate();
        }

        return "ok";
    }};
    auto cancel_request_handler{
        [](const core::CancelOrderRequestContainer& cancel_request) -> std::string {
            return cancel_request.order_id.has_value()
                       ? "ok"
                       : "Cancel request original client order ID not found";
        }};
    auto catch_all_handler{[](const auto&) -> std::string {
        logger->error("[OM] Received unexpected request from Gateway");

        return "Unsupported request type";
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

void forward_and_reply(bool is_container_valid, const core::Container& container,
                       const OrderManager::OrderInfoMapContainer& order_info_map,
                       int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                       int order_request_connection_id, transport::InboundServer& inbound_ws_server,
                       const std::optional<std::string_view>& order_reject_reason) {
    boost::contract::check c = boost::contract::function().precondition([&] {
        // Only provide reject reason when the container is invalid
        if (is_container_valid) {
            BOOST_CONTRACT_ASSERT(!order_reject_reason.has_value());
        } else {
            BOOST_CONTRACT_ASSERT(order_reject_reason.has_value());
        }
    });

    if (is_container_valid) {
        // Forward validated container to Matching Engine
        const auto message =
            std::visit([](auto&& c) { return transport::serialize_container(c); }, container);
        order_request_ws_client.send(order_request_connection_id, message)
            .transform([] { logger->info("[OM] Forwarded a valid container"); })
            .transform_error([](int err) -> int {
                logger->error("[OM] Failed to forward a valid container");

                return err;
            });

        // Gives immediate execution report as response back to broker
        const auto execution_report = generate_success_report_container(container, order_info_map);

        inbound_ws_server
            .send(arrival_gateway_id, transport::serialize_container(execution_report),
                  transport::MessageFormat::binary)
            .transform([&] {
                logger->info("[OM] Replied a success execution report: {}", execution_report);
            })
            .transform_error([&](int err) -> int {
                logger->error("[OM] Failed to reply a success execution report: {}",
                              execution_report);

                return err;
            });
    } else {
        // Give immediate execution report as feedback on rejection to broker
        const auto execution_report = generate_rejection_report_container(
            container, order_info_map, order_reject_reason.value());

        inbound_ws_server
            .send(arrival_gateway_id, transport::serialize_container(execution_report),
                  transport::MessageFormat::binary)
            .transform([&] {
                logger->info("[OM] Replied a rejection execution report: {}", execution_report);
            })
            .transform_error([&](int err) -> int {
                logger->error("[OM] Failed to reply a rejection execution report: {}",
                              execution_report);

                return err;
            });
    }
}

core::ExecutionReportContainer
generate_rejection_report_container(const core::Container& container,
                                    const OrderManager::OrderInfoMapContainer& order_info_store,
                                    std::string_view order_reject_reason) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        return core::ExecutionReportContainer{
            .sender_comp_id = SERVER_NAME,
            .target_comp_id = new_order.sender_comp_id,
            .order_id = new_order.order_id.value(),
            .cl_order_id = new_order.cl_ord_id,
            .orig_cl_ord_id = std::nullopt,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_rejected,
            .ord_status = core::OrderStatus::status_rejected,
            .text = static_cast<std::string>(order_reject_reason),
            .symbol = new_order.symbol,
            .side = new_order.side,
            .price = new_order.price,
            .time_in_force = new_order.time_in_force,
            .leaves_qty = 0,
            .cum_qty = 0,
            .avg_px = 0};
    }};

    auto cancel_request_handler{[&](const core::CancelOrderRequestContainer& cancel_request) {
        const auto order_info =
            cancel_request.order_id
                .transform([&](int order_id) -> OrderInfo { return order_info_store.at(order_id); })
                .or_else([]() -> std::optional<OrderInfo> { return OrderInfo{}; })
                .value();

        return core::ExecutionReportContainer{
            .sender_comp_id = SERVER_NAME,
            .target_comp_id = cancel_request.sender_comp_id,
            .order_id = cancel_request.order_id.value_or(-1),
            .cl_order_id = cancel_request.cl_ord_id,
            .orig_cl_ord_id = cancel_request.orig_cl_ord_id,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_rejected,
            .ord_status = core::OrderStatus::status_rejected,
            .text = static_cast<std::string>(order_reject_reason),
            .symbol = order_info.symbol,
            .side = order_info.side,
            .price = order_info.price,
            .time_in_force = order_info.time_in_force,
            .leaves_qty = 0,
            .cum_qty = order_info.cum_qty,
            .avg_px = order_info.avg_px};
    }};

    auto catch_all_handler{[](const auto&) {
        logger->error("Unreachable");

        std::terminate();
        return core::ExecutionReportContainer{};
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

core::ExecutionReportContainer
generate_success_report_container(const core::Container& container,
                                  const OrderManager::OrderInfoMapContainer& order_info_map) {
    auto new_order_handler{[](const core::NewOrderSingleContainer& new_order) {
        return core::ExecutionReportContainer{
            .sender_comp_id = SERVER_NAME,
            .target_comp_id = new_order.sender_comp_id,
            .order_id = new_order.order_id.value(),
            .cl_order_id = new_order.cl_ord_id,
            .orig_cl_ord_id = std::nullopt,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_new,
            .ord_status = core::OrderStatus::status_new,
            .text = std::nullopt,
            .symbol = new_order.symbol,
            .side = new_order.side,
            .price = new_order.price,
            .time_in_force = new_order.time_in_force,
            .leaves_qty = new_order.order_qty,
            .cum_qty = 0,
            .avg_px = 0};
    }};

    auto cancel_request_handler{[&](const core::CancelOrderRequestContainer& cancel_request) {
        const auto& order_info = order_info_map.at(cancel_request.order_id.value());
        return core::ExecutionReportContainer{
            .sender_comp_id = SERVER_NAME,
            .target_comp_id = cancel_request.sender_comp_id,
            .order_id = cancel_request.order_id.value(),
            .cl_order_id = cancel_request.cl_ord_id,
            .orig_cl_ord_id = cancel_request.orig_cl_ord_id,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_pending_cancel,
            .ord_status = core::OrderStatus::status_pending_cancel,
            .text = std::nullopt,
            .symbol = order_info.symbol,
            .side = order_info.side,
            .price = order_info.price,
            .time_in_force = order_info.time_in_force,
            .leaves_qty = order_info.leaves_qty,
            .cum_qty = order_info.cum_qty,
            .avg_px = order_info.avg_px};
    }};

    auto catch_all_handler{[](const auto&) {
        assert(false && "Unreachable");
        return core::ExecutionReportContainer{};
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

void update_database(const core::Container& container, int server_id,
                     const OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                     const BalanceChecker& balance_checker, OrderManagerDatabase& database_client,
                     std::optional<bool> valid_container) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        boost::contract::check c = boost::contract::function().precondition([&] {
            BOOST_CONTRACT_ASSERT(new_order.order_id.has_value());
            BOOST_CONTRACT_ASSERT(valid_container.has_value());
        });

        logger->info("Persisting New Order: {}", new_order);

        database_client.insert_order(new_order.order_id.value(), new_order,
                                     valid_container.value());
    }};

    auto cancel_request_handler{[&](const core::CancelOrderRequestContainer& cancel_request) {
        boost::contract::check c = boost::contract::function().precondition(
            [&] { BOOST_CONTRACT_ASSERT(valid_container.has_value()); });

        logger->info("Persisting Cancel Request: {}", cancel_request);
        database_client.insert_cancel_request(cancel_request, valid_container.value());
    }};

    auto execution_report_handler{[&](const core::ExecutionReportContainer& execution_report) {
        // Disabled for now until we decide on whether to persist execution reports
        // database_client.insert_execution(execution_report);
    }};

    auto trade_handler{[&](const core::TradeContainer& trade) {
        logger->info("Persisting Trade: {}", trade);
        database_client.insert_trade(trade);

        const auto& symbol = trade.ticker;
        const auto& buyer_id = trade.is_taker_buyer ? trade.taker_id : trade.maker_id;
        const int buyer_user_id = username_user_id_map.at(buyer_id);

        database_client.update_balance(server_id, buyer_user_id, USD_SYMBOL,
                                       balance_checker.get_balance(buyer_id, USD_SYMBOL));
        database_client.update_balance(server_id, buyer_user_id, symbol,
                                       balance_checker.get_balance(buyer_id, symbol));

        const auto& seller_id = trade.is_taker_buyer ? trade.maker_id : trade.taker_id;
        const int seller_user_id = username_user_id_map.at(seller_id);

        database_client.update_balance(server_id, seller_user_id, USD_SYMBOL,
                                       balance_checker.get_balance(seller_id, USD_SYMBOL));
        database_client.update_balance(server_id, seller_user_id, symbol,
                                       balance_checker.get_balance(seller_id, symbol));
    }};

    auto cancel_response_handler{[&](const core::CancelOrderResponseContainer& cancel_response) {
        logger->info("Persisting Cancel Response: {}", cancel_response);

        database_client.insert_cancel_response(cancel_response);
    }};

    auto catch_all_handler{[&](const auto&) { assert(false && "UNREACHABLE"); }};

    std::visit(overloaded{new_order_handler, cancel_request_handler, execution_report_handler,
                          trade_handler, cancel_response_handler, catch_all_handler},
               container);
}

void update_internal_data(const core::Container& container,
                          OrderManager::OrderInfoMapContainer& order_info_map,
                          BalanceChecker& balance_checker) {
    auto trade_handler{[&](const core::TradeContainer& trade) {
        boost::contract::check c = boost::contract::function().precondition([&] {
            BOOST_CONTRACT_ASSERT(order_info_map.contains(trade.taker_order_id));
            BOOST_CONTRACT_ASSERT(order_info_map.contains(trade.maker_order_id));
        });

        auto& taker_order_info = order_info_map.at(trade.taker_order_id);
        taker_order_info.avg_px =
            (taker_order_info.avg_px * taker_order_info.cum_qty + trade.price * trade.quantity) /
            (trade.quantity + taker_order_info.cum_qty);
        taker_order_info.leaves_qty -= trade.quantity;
        taker_order_info.cum_qty += trade.quantity;

        auto& maker_order_info = order_info_map.at(trade.maker_order_id);
        maker_order_info.avg_px =
            (maker_order_info.avg_px * maker_order_info.cum_qty + trade.price * trade.quantity) /
            (trade.quantity + maker_order_info.cum_qty);
        maker_order_info.leaves_qty -= trade.quantity;
        maker_order_info.cum_qty += trade.quantity;

        // Buyer's stock balance should increase from buying
        const auto& buyer_id = trade.is_taker_buyer ? trade.taker_id : trade.maker_id;
        balance_checker.update_balance(buyer_id, trade.ticker, trade.quantity);

        // A limit buyer may pay a lower price than the original order stated, refund the
        // excess deducted balance
        const auto buyer_order_id =
            trade.is_taker_buyer ? trade.taker_order_id : trade.maker_order_id;

        order_info_map.at(buyer_order_id).price.transform([&](int price) {
            const auto price_improvement = price - trade.price;
            assert(price_improvement >= 0 && "Only price improvement should happen");

            if (price_improvement > 0)
                balance_checker.update_balance(buyer_id, USD_SYMBOL,
                                               price_improvement * trade.quantity);

            return price;
        });

        // Seller's USD balance should increase from selling stocks
        const auto& seller_id = trade.is_taker_buyer ? trade.maker_id : trade.taker_id;
        balance_checker.update_balance(seller_id, USD_SYMBOL, trade.price * trade.quantity);
    }};

    auto cancel_response_handler{[&](const core::CancelOrderResponseContainer& cancel_response) {
        if (cancel_response.success) {
            const auto& order_info = order_info_map.at(cancel_response.order_id);

            if (order_info.side == core::Side::bid) {
                assert(order_info.price.has_value() && "Only limit order should be cancellable");

                balance_checker.update_balance(order_info.sender_comp_id, USD_SYMBOL,
                                               order_info.price.value() * order_info.leaves_qty);
            } else {
                assert(order_info.price.has_value() && "Only limit order should be cancellable");

                balance_checker.update_balance(order_info.sender_comp_id, order_info.symbol,
                                               order_info.leaves_qty);
            }
        }
    }};

    auto catch_all_handler{[](const auto&) {
        logger->error("Unreachable");

        std::terminate();
    }};

    std::visit(overloaded{trade_handler, cancel_response_handler, catch_all_handler}, container);
}

void return_execution_report(const core::Container& container,
                             const OrderManager::OrderIdMapContainer& order_id_map,
                             const OrderManager::OrderInfoMapContainer& order_info_map,
                             transport::InboundServer& inbound_ws_server) {
    auto trade_handler{[&](const core::TradeContainer& trade) {
        const auto [taker_exec_report, maker_exec_report] =
            generate_matched_order_report_containers(trade, order_id_map, order_info_map);

        const int taker_order_arrival_gateway_id{
            order_info_map.at(trade.taker_order_id).arrival_gateway_id};
        inbound_ws_server
            .send(taker_order_arrival_gateway_id, transport::serialize_container(taker_exec_report))
            .transform([&] {
                logger->info("Successfully returned execution report: {}", taker_exec_report);
            })
            .transform_error([&](int err) {
                logger->error("Failed to returned execution report: {}", taker_exec_report);

                return err;
            });

        const int maker_order_arrival_gateway_id{
            order_info_map.at(trade.maker_order_id).arrival_gateway_id};
        inbound_ws_server
            .send(maker_order_arrival_gateway_id, transport::serialize_container(maker_exec_report))
            .transform([&] {
                logger->info("Successfully returned execution report: {}", maker_exec_report);
            })
            .transform_error([&](int err) {
                logger->error("Failed to returned execution report: {}", maker_exec_report);

                return err;
            });
    }};
    auto cancel_response_handler{[&](const core::CancelOrderResponseContainer& cancel_response) {
        const auto exec_report = generate_cancel_response_report_container(
            cancel_response, order_id_map, order_info_map);
        const int orig_order_arrival_gateway_id{
            order_info_map.at(cancel_response.order_id).arrival_gateway_id};
        inbound_ws_server
            .send(orig_order_arrival_gateway_id, transport::serialize_container(exec_report))
            .transform(
                [&] { logger->info("Successfully returned execution report: {}", exec_report); })
            .transform_error([&](int err) {
                logger->error("Failed to returned execution report: {}", exec_report);

                return err;
            });
    }};
    auto catch_all_handler{[](const auto&) {
        logger->error("Unreachable");

        std::terminate();
    }};

    std::visit(overloaded{trade_handler, cancel_response_handler, catch_all_handler}, container);
}

// Generates (Taker Order Execution Report, Maker Order Execution Report)
std::pair<core::ExecutionReportContainer, core::ExecutionReportContainer>
generate_matched_order_report_containers(
    const core::TradeContainer& trade, const OrderManager::OrderIdMapContainer& order_id_map,
    const OrderManager::OrderInfoMapContainer& order_info_map) {
    const core::ExecutionReportContainer taker_order_report_container{
        .sender_comp_id = SERVER_NAME,
        .target_comp_id = trade.taker_id,
        .order_id = trade.taker_order_id,
        .cl_order_id = order_id_map.left.at(trade.taker_order_id) / core::constants::max_user_count,
        .exec_id = to_string(boost::uuids::time_generator_v7()()),
        .exec_trans_type = core::ExecTransType::exec_trans_new,
        .exec_type = (order_info_map.at(trade.taker_order_id).leaves_qty == 0)
                         ? core::ExecType::status_filled
                         : core::ExecType::status_partially_filled,
        .ord_status = (order_info_map.at(trade.taker_order_id).leaves_qty == 0)
                          ? core::OrderStatus::status_filled
                          : core::OrderStatus::status_partially_filled,
        .symbol = trade.ticker,
        .side = (trade.is_taker_buyer) ? core::Side::bid : core::Side::ask,
        .price = trade.price,
        .time_in_force = order_info_map.at(trade.taker_order_id).time_in_force,
        .leaves_qty = order_info_map.at(trade.taker_order_id).leaves_qty,
        .cum_qty = order_info_map.at(trade.taker_order_id).cum_qty,
        .avg_px = order_info_map.at(trade.taker_order_id).avg_px};

    const core::ExecutionReportContainer maker_order_report_container{
        .sender_comp_id = SERVER_NAME,
        .target_comp_id = trade.maker_id,
        .order_id = trade.maker_order_id,
        .cl_order_id = order_id_map.left.at(trade.maker_order_id) / core::constants::max_user_count,
        .exec_id = to_string(boost::uuids::time_generator_v7()()),
        .exec_trans_type = core::ExecTransType::exec_trans_new,
        .exec_type = (order_info_map.at(trade.maker_order_id).leaves_qty == 0)
                         ? core::ExecType::status_filled
                         : core::ExecType::status_partially_filled,
        .ord_status = (order_info_map.at(trade.maker_order_id).leaves_qty == 0)
                          ? core::OrderStatus::status_filled
                          : core::OrderStatus::status_partially_filled,
        .symbol = trade.ticker,
        .side = (trade.is_taker_buyer) ? core::Side::ask : core::Side::bid,
        .price = trade.price,
        .time_in_force = order_info_map.at(trade.maker_order_id).time_in_force,
        .leaves_qty = order_info_map.at(trade.maker_order_id).leaves_qty,
        .cum_qty = order_info_map.at(trade.maker_order_id).cum_qty,
        .avg_px = order_info_map.at(trade.maker_order_id).avg_px};

    return std::pair{std::move(taker_order_report_container),
                     std::move(maker_order_report_container)};
}

core::ExecutionReportContainer generate_cancel_response_report_container(
    const core::CancelOrderResponseContainer& cancel_response,
    const OrderManager::OrderIdMapContainer& order_id_map,
    const OrderManager::OrderInfoMapContainer& order_info_map) {
    return core::ExecutionReportContainer{
        .sender_comp_id = SERVER_NAME,
        .target_comp_id = order_info_map.at(cancel_response.order_id).sender_comp_id,
        .order_id = cancel_response.order_id,
        .cl_order_id = cancel_response.cl_ord_id,
        .orig_cl_ord_id =
            order_id_map.left.at(cancel_response.order_id) / core::constants::max_user_count,
        .exec_id = to_string(boost::uuids::time_generator_v7()()),
        .exec_trans_type = core::ExecTransType::exec_trans_new,
        .exec_type = (cancel_response.success) ? core::ExecType::status_canceled
                                               : core::ExecType::status_rejected,
        .ord_status = (cancel_response.success) ? core::OrderStatus::status_canceled
                                                : core::OrderStatus::status_rejected,
        .text = cancel_response.success ? "" : "Order had already been matched",
        .symbol = order_info_map.at(cancel_response.order_id).symbol,
        .side = order_info_map.at(cancel_response.order_id).side,
        .price = order_info_map.at(cancel_response.order_id).price,
        .time_in_force = order_info_map.at(cancel_response.order_id).time_in_force,
        .leaves_qty = 0,
        .cum_qty = order_info_map.at(cancel_response.order_id).cum_qty,
        .avg_px = order_info_map.at(cancel_response.order_id).avg_px};
};
} // namespace om
