#include "matching_engine.h"

#include "core/containers.h"
#include "shared_memory_publisher.h"
#include "transport/messaging.h"

namespace engine {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

static std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
    "matching_engine_logger",
    std::format("{}/logs/{}/matching_engine.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME));

MatchingEngine::MatchingEngine(std::string_view host, int port,
                               const std::vector<std::string>& active_symbols,
                               const std::chrono::milliseconds flush_interval,
                               const MatchingEngineDependencyFactory& dependency_factory)
    : inbound_server{dependency_factory.create_inbound_server(host, port, logger)},
      flush_interval{flush_interval}, incoming_request_connection_id{},
      order_response_connection_id{} {

    for (const auto& symbol : active_symbols) {
        limit_order_books.emplace(
            symbol,

            LimitOrderBook{symbol, this->trade_events,
                           dependency_factory.create_trade_publisher(symbol)});

        orderbook_snapshot_publishers.emplace(
            symbol,

            dependency_factory.create_orderbook_snapshot_publisher(symbol));
    }
}

void MatchingEngine::init() {
    // TODO: Handle websocket start error after Websocket's error handling is updated
    inbound_server->start();

    logger->info("Matching Engine starts accepting connections");
    logger->flush();
}

// Spin locks until matching engine has two connections from OMS
void MatchingEngine::wait_for_connections() {
    auto connection_info = inbound_server->get_connection_info();
    while (connection_info.size() != 2) {
        connection_info = inbound_server->get_connection_info();
    }

    for (const auto& [id, counter_party] : connection_info) {
        if (counter_party == "order_request") {
            incoming_request_connection_id = id;
            logger->info("Order request connection established, id: {}",
                         incoming_request_connection_id);
        } else if (counter_party == "order_response") {
            order_response_connection_id = id;
            logger->info("Order response connection established, id: {}",
                         order_response_connection_id);
        } else {
            logger->error("Unexpected connection: {}", counter_party);
        }
    }
}

void MatchingEngine::run() {
    auto last_flush = std::chrono::steady_clock::now();

    while (true) {
        if (auto new_message = inbound_server->dequeue_message(incoming_request_connection_id);
            new_message.has_value()) {
            const auto container = transport::deserialize_container(new_message.value());

            auto new_order_handler{[this](const core::NewOrderSingleContainer& new_order) {
                assert(new_order.order_id.has_value());

                auto& limit_order_book = limit_order_books.at(new_order.symbol);

                limit_order_book.add_order(new_order.order_id.value(),
                                           new_order.price.value_or((new_order.side == Side::bid)
                                                                        ? MARKET_BID_ORDER_PRICE
                                                                        : MARKET_ASK_ORDER_PRICE),
                                           new_order.order_qty,
                                           (new_order.side == Side::bid) ? Side::bid : Side::ask,
                                           new_order.sender_comp_id);

                logger->info("New order request received");
                logger->info("New order quantity: {}", new_order.order_qty);
                logger->flush();

                while (!trade_events.empty()) {
                    const auto current_trade = trade_events.front();

                    std::cout << "qty at trade container creation: " << current_trade.quantity
                              << std::endl;

                    const auto trade_container =
                        core::TradeContainer{.ticker = current_trade.ticker,
                                             .price = current_trade.price,
                                             .quantity = current_trade.quantity,
                                             .trade_id = current_trade.trade_id,
                                             .taker_id = current_trade.taker_id,
                                             .maker_id = current_trade.maker_id,
                                             .taker_order_id = current_trade.taker_order_id,
                                             .maker_order_id = current_trade.maker_order_id};

                    inbound_server->send(order_response_connection_id,
                                         transport::serialize_container(trade_container));
                    trade_events.pop();
                }
            }};
            auto cancel_order_handler{[this](
                                          const core::CancelOrderRequestContainer& cancel_request) {
                logger->info("Cancel order request received");
                logger->info(transport::serialize_container(cancel_request));
                logger->flush();

                auto& limit_order_book = limit_order_books.at(cancel_request.symbol);

                bool cancel_success = true;
                if (limit_order_book.order_id_exists(cancel_request.order_id.value())) {
                    limit_order_book.cancel_order(cancel_request.order_id.value());
                } else {
                    cancel_success = false;
                }

                const auto cancel_response =
                    core::CancelOrderResponseContainer{.order_id = cancel_request.order_id.value(),
                                                       .cl_ord_id = cancel_request.cl_ord_id,
                                                       .success = cancel_success};

                inbound_server->send(order_response_connection_id,
                                     transport::serialize_container(cancel_response));
            }};
            auto fill_cost_query_handler{
                [this](const core::FillCostQueryContainer& fill_cost_query) {
                    logger->info("Fill cost query received");
                    logger->info("Quantity: {}", fill_cost_query.quantity);
                    logger->flush();

                    auto& limit_order_book = limit_order_books.at(fill_cost_query.symbol);
                    auto total_cost = limit_order_book.get_fill_cost(fill_cost_query.quantity,
                                                                     fill_cost_query.side);

                    logger->info("Calculated fill cost: {}", total_cost.value_or(-1));

                    const auto response_container = core::FillCostResponseContainer{
                        total_cost.transform([](int v) { return std::optional{v}; })
                            .value_or(std::nullopt)};

                    inbound_server->send(incoming_request_connection_id,
                                         transport::serialize_container(response_container));
                }};
            auto catch_all_handler{[this](auto&&) {
                logger->error("Received unexpected request from Order Manager");
                logger->flush();
            }};

            std::visit(overloaded{new_order_handler, cancel_order_handler, fill_cost_query_handler,
                                  catch_all_handler},
                       container);

            if (const auto now{std::chrono::steady_clock::now()};
                now - last_flush > flush_interval) {
                for (const auto& [symbol, lob] : limit_order_books) {
                    auto snapshot{lob.get_top_order_book_level_aggregate()};

                    if (auto& symbol_snapshot_publisher = orderbook_snapshot_publishers.at(symbol);
                        !symbol_snapshot_publisher->try_publish(snapshot)) {
                        std::cerr << "Failed to push snapshot " << "\n";
                    }
                }
                last_flush = now;
            }
        }
    }
}
} // namespace engine