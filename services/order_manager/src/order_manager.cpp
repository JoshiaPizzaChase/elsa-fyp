#include "order_manager.h"
#include "transport/messaging.h"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <boost/contract.hpp>
#include <boost/uuid.hpp>

namespace om {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

static std::shared_ptr<spdlog::logger> logger{spdlog::basic_logger_mt<spdlog::async_factory>(
    "order_manager_logger",
    std::format("{}/logs/{}/order_manager.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME))};

OrderManager::OrderManager(std::string_view host, int port,
                           const OrderManagerDependencyFactory& dependency_factory)
    : order_request_connection_id{-1}, order_response_connection_id{-1},
      inbound_server{
          dependency_factory.create_inbound_server(host, port, logger, gateway_connection_ids)},
      order_request_outbound_client{dependency_factory.create_outbound_client(logger)},
      order_response_outbound_client{dependency_factory.create_outbound_client(logger)},
      database_client{dependency_factory.create_database_client(true)} {
}

void OrderManager::init() {
    std::ignore = inbound_server->start()
                      .transform([] {
                          logger->info("[OM] Order Manager starts accepting connections");
                          logger->flush();
                      })
                      .or_else([](int) -> std::expected<void, int> {
                          logger->error("[OM] Failed to start inbound connection server");
                          logger->flush();
                          std::terminate();
                      });

    std::ignore = order_request_outbound_client->start()
                      .transform([] {
                          logger->info("[OM] Order Request Client started");
                          logger->flush();
                      })
                      .or_else([](int) -> std::expected<void, int> {
                          logger->error("[OM] Order Request Client failed to start");
                          logger->flush();
                          std::terminate();
                      });

    std::ignore = order_response_outbound_client->start()
                      .transform([] {
                          logger->info("[OM] Order Response Client started");
                          logger->flush();
                      })
                      .or_else([](int) -> std::expected<void, int> {
                          logger->error("[OM] Order Response Client failed to start");
                          logger->flush();
                          std::terminate();
                      });

    init_balance_checker(balance_checker, username_user_id_map, *database_client);
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
            .or_else([](std::string_view err) -> std::expected<void, std::string> {
                logger->error("[OM] Failed to load balances into balance_checker: {}", err);
                std::terminate();
            });
}

void OrderManager::connect_matching_engine(std::string host, int port, int retry_attempts) {
    std::expected<int, int> order_request_res{};
    for (auto i{0}; i < retry_attempts; i++) {
        order_request_res = order_request_outbound_client
                                ->connect(std::format("ws://{}:{}", host, port), "order_request")
                                .transform([this](int connection_id) -> int {
                                    order_request_connection_id = connection_id;
                                    logger->info("Order Request connection established");
                                    logger->flush();
                                    return connection_id;
                                })
                                .or_else([=](int err) -> std::expected<int, int> {
                                    logger->error("Failed to establish Order Request connection, "
                                                  "attempt: {}",
                                                  i + 1);
                                    logger->flush();
                                    return std::unexpected{err};
                                });

        if (order_request_res)
            break;
    }
    assert(order_request_res.has_value() && "Order Request connection failed to establish");

    std::expected<int, int> order_response_res{};
    for (auto i{0}; i < retry_attempts; i++) {
        order_response_res = order_response_outbound_client
                                 ->connect(std::format("ws://{}:{}", host, port), "order_response")
                                 .transform([this](int connection_id) -> int {
                                     order_response_connection_id = connection_id;
                                     logger->info("Order Response connection established");
                                     logger->flush();
                                     return connection_id;
                                 })
                                 .or_else([=](int err) -> std::expected<int, int> {
                                     logger->error("Failed to establish Order Response connection, "
                                                   "attempt: {}",
                                                   i + 1);
                                     logger->flush();
                                     return std::unexpected{err};
                                 });

        if (order_response_res)
            break;
    }
    assert(order_response_res.has_value() && "Order Response connection failed to establish");
}

void OrderManager::start() {
    int curr_gateway_id = 0;

    while (true) {
        // Process an incoming request
        if (auto new_message = inbound_server->dequeue_message(curr_gateway_id);
            new_message.has_value()) {
            auto container = transport::deserialize_container(new_message.value());

            const std::optional<int> market_bid_fill_cost = preprocess_container(
                container, order_id_map, order_info_map, username_user_id_map, curr_gateway_id,
                *order_request_outbound_client, order_request_connection_id);

            const bool is_container_valid =
                validate_container(container, balance_checker, market_bid_fill_cost);

            forward_and_reply(is_container_valid, container, order_info_map, curr_gateway_id,
                              *order_request_outbound_client, order_request_connection_id,
                              *inbound_server, *database_client);

            update_database(container, *database_client, is_container_valid);
        }

        curr_gateway_id = (curr_gateway_id + 1) % gateway_connection_ids.size();

        // Process a returned message container
        if (auto new_message =
                order_response_outbound_client->dequeue_message(order_response_connection_id);
            new_message.has_value()) {
            auto container = transport::deserialize_container(new_message.value());

            if (const auto trade_container = std::get_if<core::TradeContainer>(&container)) {
                logger->info("Trade container received");
                logger->info("trade qty: {}", trade_container->quantity);
                logger->info("taker id: {}", trade_container->taker_id);
                logger->info("maker_id: {}", trade_container->maker_id);
                logger->flush();

                update_order_info(*trade_container, order_info_map);
            }

            return_execution_report(container, order_id_map, order_info_map, *inbound_server,
                                    *database_client);

            update_database(container, *database_client);
        }
    }
}

std::optional<int>
preprocess_container(core::Container& container, OrderManager::OrderIdMapContainer& order_id_map,
                     OrderManager::OrderInfoMapContainer& order_info_map,
                     const OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                     int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                     int order_request_connection_id) {
    // TODO: Fetch latest_assigned_order_id from DB
    static int latest_assigned_order_id{-1};
    auto new_order_handler{[&](core::NewOrderSingleContainer& new_order) -> std::optional<int> {
        // Assign an internal order_id to NewOrderSingleContainer
        new_order.order_id = ++latest_assigned_order_id;
        logger->info("New Order Single received: {}", new_order);

        order_id_map.insert(OrderManager::OrderIdPair(
            new_order.order_id.value(), new_order.cl_ord_id * core::constants::max_user_count +
                                            username_user_id_map.at(new_order.sender_comp_id)));

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
                                             .side = new_order.side};
            order_request_ws_client
                .send(order_request_connection_id, transport::serialize_container(fill_cost_query))
                .transform([&] {
                    logger->info("Successfully sent Fill Cost "
                                 "Query: {}",
                                 fill_cost_query);
                    logger->flush();
                });

            auto response_message =
                order_request_ws_client.wait_and_dequeue_message(order_request_connection_id);
            while (!response_message.has_value()) {
                response_message =
                    order_request_ws_client.wait_and_dequeue_message(order_request_connection_id);
            }

            const auto response_container =
                transport::deserialize_container(response_message.value());
            assert(std::holds_alternative<core::FillCostResponseContainer>(response_container) &&
                   "Unexpected container type received from Matching Engine");
            const auto fill_cost_response =
                std::get<core::FillCostResponseContainer>(response_container);
            logger->info("Fill Cost Response received: {}", fill_cost_response);

            return fill_cost_response.total_cost;
        }

        return std::nullopt;
    }};

    auto cancel_request_handler{
        [&](core::CancelOrderRequestContainer& cancel_request) -> std::optional<int> {
            // If cancel request does not have internal order_id, fill it in
            // But if the cancel request is for a non-existent orig_cl_ord_id, leave order_id blank
            if (const auto cancel_request_container =
                    std::get_if<core::CancelOrderRequestContainer>(&container);
                cancel_request_container && !cancel_request_container->order_id.has_value()) {
                if (const auto it =
                        order_id_map.right.find(cancel_request_container->orig_cl_ord_id);
                    it != order_id_map.right.end()) {
                    cancel_request_container->order_id.emplace(it->second);
                }
            }

            return std::nullopt;
        }};

    auto catch_all_handler{[](auto&) -> std::optional<int> {
        assert(false && "UNREACHABLE");
        return std::nullopt;
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

bool validate_container(const core::Container& container, BalanceChecker& balance_checker,
                        std::optional<int> market_bid_fill_cost) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        // logger->info("GME: {}",
        //              balance_checker.get_balance(new_order.sender_comp_id,
        //              new_order.symbol));
        logger->info("USD: {}", balance_checker.get_balance(new_order.sender_comp_id, USD_SYMBOL));
        logger->info("Price: {}", new_order.price.value_or(-1));
        logger->info("Quantity: {}", new_order.order_qty);
        logger->info("fill cost: {}", market_bid_fill_cost.value_or(-1));
        logger->flush();

        // A broker must at least have a record for USD at the start
        if (!balance_checker.broker_id_exists(new_order.sender_comp_id)) {
            return false;
        }

        switch (new_order.side) {
        case core::Side::bid:
            if (!balance_checker.broker_owns_ticker(new_order.sender_comp_id, USD_SYMBOL)) {
                return false;
            }

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
                assert(market_bid_fill_cost.has_value());

                if (!balance_checker.has_sufficient_balance(new_order.sender_comp_id, USD_SYMBOL,
                                                            -market_bid_fill_cost.value())) {
                    return false;
                }

                balance_checker.update_balance(new_order.sender_comp_id, USD_SYMBOL,
                                               -market_bid_fill_cost.value());

                break;
            default:
                assert(false && "Unsupported Order Type");
            }
            break;
        case core::Side::ask:
            if (!balance_checker.broker_owns_ticker(new_order.sender_comp_id, new_order.symbol)) {
                return false;
            }

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
    auto cancel_request_handler{[](const core::CancelOrderRequestContainer& cancel_request) {
        return cancel_request.order_id.has_value();
    }};
    auto catch_all_handler{[](const auto&) {
        logger->error("Received unexpected request from Gateway");
        logger->flush();
        return false;
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

void forward_and_reply(bool is_container_valid, const core::Container& container,
                       const OrderManager::OrderInfoMapContainer& order_info_map,
                       int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                       int order_request_connection_id, transport::InboundServer& inbound_ws_server,
                       OrderManagerDatabase& database_client) {
    if (is_container_valid) {
        // Forward validated container to Matching Engine
        // TODO: Handle send error
        const auto message =
            std::visit([](auto&& c) { return transport::serialize_container(c); }, container);
        order_request_ws_client.send(order_request_connection_id, message);

        logger->info("Forward a valid container");
        logger->flush();

        // Gives immediate execution report as response back to broker
        const auto execution_report = generate_success_report_container(container, order_info_map);
        inbound_ws_server.send(arrival_gateway_id, transport::serialize_container(execution_report),
                               transport::MessageFormat::binary);

        logger->info("Returned success execution report");
        logger->flush();
    } else {
        // Give immediate execution report as feedback on rejection to broker
        const auto execution_report =
            generate_rejection_report_container(container, order_info_map);
        inbound_ws_server.send(arrival_gateway_id, transport::serialize_container(execution_report),
                               transport::MessageFormat::binary);

        logger->info("Returned rejection execution report");
        logger->flush();
    }
}

core::ExecutionReportContainer
generate_rejection_report_container(const core::Container& container,
                                    const OrderManager::OrderInfoMapContainer& order_info_store) {
    auto new_order_handler{[](const core::NewOrderSingleContainer& new_order) {
        return core::ExecutionReportContainer{
            .sender_comp_id = new_order.sender_comp_id,
            .target_comp_id = "",
            .order_id = new_order.order_id.value(),
            .cl_order_id = new_order.cl_ord_id,
            .orig_cl_ord_id = std::nullopt,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_rejected,
            .ord_status = core::OrderStatus::status_rejected,
            .text = "TODO",
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
            .sender_comp_id = cancel_request.sender_comp_id,
            .target_comp_id = "",
            .order_id = cancel_request.order_id.value_or(-1),
            .cl_order_id = cancel_request.cl_ord_id,
            .orig_cl_ord_id = cancel_request.orig_cl_ord_id,
            .exec_id = to_string(boost::uuids::time_generator_v7()()),
            .exec_trans_type = core::ExecTransType::exec_trans_new,
            .exec_type = core::ExecType::status_rejected,
            .ord_status = core::OrderStatus::status_rejected,
            .text = "TODO",
            .symbol = order_info.symbol,
            .side = order_info.side,
            .price = order_info.price,
            .time_in_force = order_info.time_in_force,
            .leaves_qty = 0,
            .cum_qty = 0,
            .avg_px = 0};
    }};

    auto catch_all_handler{[](const auto&) {
        assert(false && "Unreachable");
        return core::ExecutionReportContainer{};
    }};

    return std::visit(overloaded{new_order_handler, cancel_request_handler, catch_all_handler},
                      container);
}

core::ExecutionReportContainer
generate_success_report_container(const core::Container& container,
                                  const OrderManager::OrderInfoMapContainer& order_info_store) {
    auto new_order_handler{[](const core::NewOrderSingleContainer& new_order) {
        return core::ExecutionReportContainer{
            .sender_comp_id = new_order.sender_comp_id,
            .target_comp_id = "",
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
        const auto& order_info = order_info_store.at(cancel_request.order_id.value());
        return core::ExecutionReportContainer{
            .sender_comp_id = cancel_request.sender_comp_id,
            .target_comp_id = "",
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

void update_database(const core::Container& container, OrderManagerDatabase& database_client,
                     std::optional<bool> valid_container) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        assert(new_order.order_id.has_value());
        assert(valid_container.has_value());
        database_client.insert_order(new_order.order_id.value(), new_order,
                                     valid_container.value());
    }};

    auto cancel_request_handler{[&](const core::CancelOrderRequestContainer& cancel_request) {
        assert(valid_container.has_value());
        database_client.insert_cancel_request(cancel_request, valid_container.value());
    }};

    auto execution_report_handler{[&](const core::ExecutionReportContainer& execution_report) {
        // Disabled for now until we decide on whether to persist execution reports
        // database_client.insert_execution(execution_report);
    }};

    auto trade_handler{[&](const core::TradeContainer& trade) {
        std::cout << "inserting trade into db" << std::endl;
        database_client.insert_trade(trade);
    }};

    auto cancel_response_handler{[&](const core::CancelOrderResponseContainer& cancel_response) {
        database_client.insert_cancel_response(cancel_response);
    }};

    auto catch_all_handler{[&](const auto&) { assert(false && "UNREACHABLE"); }};

    std::visit(overloaded{new_order_handler, cancel_request_handler, execution_report_handler,
                          trade_handler, cancel_response_handler, catch_all_handler},
               container);
}

void update_order_info(const core::TradeContainer& trade_container,
                       OrderManager::OrderInfoMapContainer& order_info_map) {
    auto& taker_order_info = order_info_map.at(trade_container.taker_order_id);
    taker_order_info.avg_px = (taker_order_info.avg_px * taker_order_info.cum_qty +
                               trade_container.price * trade_container.quantity) /
                              (trade_container.quantity + taker_order_info.cum_qty);
    taker_order_info.leaves_qty -= trade_container.quantity;
    taker_order_info.cum_qty += trade_container.quantity;

    auto& maker_order_info = order_info_map.at(trade_container.maker_order_id);
    maker_order_info.avg_px = (maker_order_info.avg_px * maker_order_info.cum_qty +
                               trade_container.price * trade_container.quantity) /
                              (trade_container.quantity + maker_order_info.cum_qty);
    maker_order_info.leaves_qty -= trade_container.quantity;
    maker_order_info.cum_qty += trade_container.quantity;
}

void return_execution_report(const core::Container& container,
                             const OrderManager::OrderIdMapContainer& order_id_map,
                             const OrderManager::OrderInfoMapContainer& order_info_map,
                             transport::InboundServer& inbound_ws_server,
                             OrderManagerDatabase& database_client) {
    auto trade_handler{[&](const core::TradeContainer& trade) {
        const auto [taker_exec_report, maker_exec_report] =
            generate_matched_order_report_containers(trade, order_id_map, order_info_map);

        const int taker_order_arrival_gateway_id{
            order_info_map.at(trade.taker_order_id).arrival_gateway_id};
        const int maker_order_arrival_gateway_id{
            order_info_map.at(trade.maker_order_id).arrival_gateway_id};

        inbound_ws_server.send(taker_order_arrival_gateway_id,
                               transport::serialize_container(taker_exec_report));

        inbound_ws_server.send(maker_order_arrival_gateway_id,
                               transport::serialize_container(maker_exec_report));
    }};
    auto cancel_response_handler{[&](const core::CancelOrderResponseContainer& cancel_response) {
        const auto exec_report = generate_cancel_response_report_container(
            cancel_response, order_id_map, order_info_map);
        const int orig_order_arrival_gateway_id{
            order_info_map.at(cancel_response.order_id).arrival_gateway_id};
        inbound_ws_server.send(orig_order_arrival_gateway_id,
                               transport::serialize_container(exec_report));
    }};
    auto catch_all_handler{[](const auto&) { assert(false && "UNREACHABLE"); }};

    std::visit(overloaded{trade_handler, cancel_response_handler, catch_all_handler}, container);
}

// Generates (Taker Order Execution Report, Maker Order Execution Report)
std::pair<core::ExecutionReportContainer, core::ExecutionReportContainer>
generate_matched_order_report_containers(
    const core::TradeContainer& trade, const OrderManager::OrderIdMapContainer& order_id_map,
    const OrderManager::OrderInfoMapContainer& order_info_map) {
    const core::ExecutionReportContainer taker_order_report_container{
        .sender_comp_id = trade.taker_id,
        .target_comp_id = "",
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
        .price = order_info_map.at(trade.taker_order_id).price,
        .time_in_force = order_info_map.at(trade.taker_order_id).time_in_force,
        .leaves_qty = order_info_map.at(trade.taker_order_id).leaves_qty,
        .cum_qty = order_info_map.at(trade.taker_order_id).cum_qty,
        .avg_px = order_info_map.at(trade.taker_order_id).avg_px};

    const core::ExecutionReportContainer maker_order_report_container{
        .sender_comp_id = trade.maker_id,
        .target_comp_id = "",
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
        .price = order_info_map.at(trade.maker_order_id).price,
        .time_in_force = order_info_map.at(trade.maker_order_id).time_in_force,
        .leaves_qty = order_info_map.at(trade.maker_order_id).leaves_qty,
        .cum_qty = order_info_map.at(trade.maker_order_id).cum_qty,
        .avg_px = order_info_map.at(trade.maker_order_id).avg_px};

    return std::pair{taker_order_report_container, maker_order_report_container};
}

core::ExecutionReportContainer generate_cancel_response_report_container(
    const core::CancelOrderResponseContainer& cancel_response,
    const OrderManager::OrderIdMapContainer& order_id_map,
    const OrderManager::OrderInfoMapContainer& order_info_map) {
    return core::ExecutionReportContainer{
        .sender_comp_id = order_info_map.at(cancel_response.order_id).sender_comp_id,
        .target_comp_id = "",
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
        .text = "TODO",
        .symbol = order_info_map.at(cancel_response.order_id).symbol,
        .side = order_info_map.at(cancel_response.order_id).side,
        .price = order_info_map.at(cancel_response.order_id).price,
        .time_in_force = order_info_map.at(cancel_response.order_id).time_in_force,
        .leaves_qty = 0,
        .cum_qty = order_info_map.at(cancel_response.order_id).cum_qty,
        .avg_px = order_info_map.at(cancel_response.order_id).avg_px};
};
} // namespace om
