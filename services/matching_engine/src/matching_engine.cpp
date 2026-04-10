#include "matching_engine.h"
#include "core/containers.h"
#include "shared_memory_publisher.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "transport/messaging.h"

namespace engine {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

auto _ = [] {
    spdlog::cfg::load_env_levels();
    spdlog::flush_every(std::chrono::seconds{5});
    return 0;
}();
static std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
    "matching_engine_logger",
    std::format("{}/logs/{}/matching_engine.log", std::string(PROJECT_SOURCE_DIR), SERVER_NAME));

MatchingEngine::MatchingEngine(std::string_view host, int port,
                               const std::vector<std::string>& active_symbols,
                               const std::chrono::milliseconds flush_interval,
                               const MatchingEngineDependencyFactory& dependency_factory)
    : incoming_request_connection_id{-1}, order_response_connection_id{-1},
      inbound_server{dependency_factory.create_inbound_server(
          host, port, logger, incoming_request_connection_id, order_response_connection_id)},
      flush_interval{flush_interval} {

    for (const auto& symbol : active_symbols) {
        limit_order_books.emplace(
            symbol, LimitOrderBook{symbol, this->trade_events,
                                   dependency_factory.create_trade_publisher(symbol)});

        orderbook_snapshot_publishers.emplace(
            symbol, dependency_factory.create_orderbook_snapshot_publisher(symbol));
    }
}

void MatchingEngine::init() const {
    std::ignore =
        inbound_server->start()
            .transform([] { logger->info("[ME] Matching Engine starts accepting connections"); })
            .or_else([](int) -> std::expected<void, int> {
                logger->error("[ME] Failed to start inbound websocket server");

                std::terminate();
                return {};
            });
}

// Spin locks until matching engine has two connections from OMS
void MatchingEngine::wait_for_connections() const {
    while (incoming_request_connection_id == -1 || order_response_connection_id == -1) {
    }
    logger->info("Both connections from Order Manager have been established");
}

void MatchingEngine::run() {
    auto last_flush = std::chrono::steady_clock::now();

    while (true) {
        std::ignore =
            inbound_server->dequeue_message(incoming_request_connection_id)
                .transform([&](std::string&& new_message) -> std::optional<std::string> {
                    const auto container = transport::deserialize_container(new_message);

                    process_container(container, limit_order_books, trade_events, *inbound_server,
                                      order_response_connection_id, incoming_request_connection_id);

                    return new_message;
                });

        if (const auto now{std::chrono::steady_clock::now()}; now - last_flush > flush_interval) {
            for (const auto& [symbol, lob] : limit_order_books) {
                auto snapshot{lob.get_top_order_book_level_aggregate()};

                if (auto& symbol_snapshot_publisher = orderbook_snapshot_publishers.at(symbol);
                    !symbol_snapshot_publisher->try_publish(snapshot)) {
                    logger->error("Failed to push snapshot");
                }
            }
            last_flush = now;
        }
    }
}

void process_container(const core::Container& container,
                       std::unordered_map<std::string, LimitOrderBook>& limit_order_books,
                       std::queue<Trade>& trade_events, transport::InboundServer& inbound_server,
                       int order_response_connection_id, int incoming_request_connection_id) {
    auto new_order_handler{[&](const core::NewOrderSingleContainer& new_order) {
        boost::contract::check c = boost::contract::function().precondition(
            [&] { BOOST_CONTRACT_ASSERT(new_order.order_id.has_value()); });

        logger->info("[ME] New order received: {}", new_order);

        auto& limit_order_book = limit_order_books.at(new_order.symbol);

        limit_order_book.add_order(
            new_order.order_id.value(),
            new_order.price.value_or((new_order.side == Side::bid) ? MARKET_BID_ORDER_PRICE
                                                                   : MARKET_ASK_ORDER_PRICE),
            new_order.order_qty, (new_order.side == Side::bid) ? Side::bid : Side::ask,
            new_order.sender_comp_id);

        while (!trade_events.empty()) {
            const auto current_trade = trade_events.front();

            const auto trade_container =
                core::TradeContainer{.ticker = current_trade.ticker,
                                     .price = current_trade.price,
                                     .quantity = current_trade.quantity,
                                     .trade_id = current_trade.trade_id,
                                     .taker_id = current_trade.taker_id,
                                     .maker_id = current_trade.maker_id,
                                     .taker_order_id = current_trade.taker_order_id,
                                     .maker_order_id = current_trade.maker_order_id,
                                     .is_taker_buyer = current_trade.is_taker_buyer};

            const auto res =
                inbound_server
                    .send(order_response_connection_id,
                          transport::serialize_container(trade_container))
                    .transform(
                        [&] { logger->info("[ME] Successfully sent Trade: {}", trade_container); })
                    .or_else([&](int) -> std::expected<void, int> {
                        logger->error("[ME] Failed to sent Trade: {}", trade_container);

                        return std::unexpected{-1};
                    });

            if (!res.has_value()) {
                break;
            }

            trade_events.pop();
        }
    }};
    auto cancel_order_handler{[&](const core::CancelOrderRequestContainer& cancel_request) {
        boost::contract::check c = boost::contract::function().precondition(
            [&] { BOOST_CONTRACT_ASSERT(cancel_request.order_id.has_value()); });

        logger->info("[ME] Cancel request received: {}", cancel_request);

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

        std::ignore =
            inbound_server
                .send(order_response_connection_id, transport::serialize_container(cancel_response))
                .transform([&] {
                    logger->info("[ME] Successfully sent Cancel Response: {}", cancel_response);
                })
                .or_else([&](int) -> std::expected<void, int> {
                    logger->error("[ME] Failed to sent Cancel Response: {}", cancel_response);

                    return std::unexpected{-1};
                });
    }};
    auto fill_cost_query_handler{[&](const core::FillCostQueryContainer& fill_cost_query) {
        logger->info("[ME] Fill cost query received: {}", fill_cost_query);

        const auto& limit_order_book = limit_order_books.at(fill_cost_query.symbol);
        auto total_cost =
            limit_order_book.get_fill_cost(fill_cost_query.quantity, fill_cost_query.side);

        const auto response_container = core::FillCostResponseContainer{std::move(total_cost)};

        std::ignore =
            inbound_server
                .send(incoming_request_connection_id,
                      transport::serialize_container(response_container))
                .transform([&] {
                    logger->info("[ME] Successfully sent Fill Cost Response: {}",
                                 response_container);
                })
                .or_else([&](int) -> std::expected<void, int> {
                    logger->error("[ME] Failed to sent Fill Cost Response: {}", response_container);

                    return std::unexpected{-1};
                });
    }};
    auto catch_all_handler{
        [](auto&&) { logger->error("Received unexpected request from Order Manager"); }};

    std::visit(overloaded{new_order_handler, cancel_order_handler, fill_cost_query_handler,
                          catch_all_handler},
               container);
}

const std::unordered_map<std::string, LimitOrderBook>&
MatchingEngine::get_limit_order_books() const {
    return limit_order_books;
}

const std::unordered_map<std::string, std::unique_ptr<Publisher<TopOrderBookLevelAggregates>>>&
MatchingEngine::get_snapshot_publishers() const {
    return orderbook_snapshot_publishers;
}
} // namespace engine