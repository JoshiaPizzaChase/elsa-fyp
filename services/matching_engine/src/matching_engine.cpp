#include "matching_engine.h"
#include "transport/messaging.h"

namespace engine {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

MatchingEngine::MatchingEngine(std::string_view host, int port)
    : logger{spdlog::basic_logger_mt<spdlog::async_factory>(
          "matching_engine_logger", std::string{PROJECT_SOURCE_DIR} + "/logs/matching_engine.log")},
      inbound_ws_server{port, host, logger},
      shm_orderbook_snapshot{OrderbookSnapshotRingBuffer::open_exist_shm(
          core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE)},
      incoming_request_connection_id{}, order_response_connection_id{}, limit_order_book{"GME"},
      latest_order_id{0} {
    inbound_ws_server.start();

    logger->info("Matching Engine constructed");
    logger->flush();
}

// Spin locks until matching engine has two connections from OMS
void MatchingEngine::wait_for_connections() {
    auto id_to_connection_map = inbound_ws_server.get_id_to_connection_map();
    while (id_to_connection_map.size() != 2) {
        id_to_connection_map = inbound_ws_server.get_id_to_connection_map();
    }

    for (const auto& [id, connection_metadata] : id_to_connection_map) {
        if (auto counter_party = connection_metadata->get_counter_party();
            counter_party == "order_request") {
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

std::expected<void, std::string> MatchingEngine::start() {
    while (true) {
        if (auto new_message = inbound_ws_server.dequeue_message(incoming_request_connection_id);
            new_message.has_value()) {
            const auto container = transport::deserialize_container(new_message.value());

            auto new_order_handler{[this](const core::NewOrderSingleContainer& new_order) {
                limit_order_book.add_order(latest_order_id++,
                                           new_order.price.value_or((new_order.side == Side::bid)
                                                                        ? MARKET_BID_ORDER_PRICE
                                                                        : MARKET_ASK_ORDER_PRICE),
                                           new_order.order_qty,
                                           (new_order.side == Side::bid) ? Side::bid : Side::ask);

                logger->info("New order request received");
                logger->info("New order quantity: {}", new_order.order_qty);
                logger->flush();
            }};
            auto cancel_order_handler{
                [this](const core::CancelOrderRequestContainer& cancel_request) {
                    // TODO: Handle conversion from clorid to internal id then cancel
                    // limit_order_book.cancel_order();

                    logger->info("Cancel order request received");
                    logger->info(transport::serialize_container(cancel_request));
                    logger->flush();
                }};
            auto fill_cost_query_handler{
                [this](const core::FillCostQueryContainer& fill_cost_query) {
                    logger->info("Fill cost query received");
                    logger->info("Quantity: {}", fill_cost_query.quantity);
                    logger->flush();

                    auto total_cost = limit_order_book.get_fill_cost(fill_cost_query.quantity,
                                                                     fill_cost_query.side);

                    logger->info("Calculated fill cost: {}", total_cost.value_or(-1));

                    const auto response_container = core::FillCostResponseContainer{
                        total_cost.transform([](int v) { return std::optional{v}; })
                            .value_or(std::nullopt)};

                    inbound_ws_server.send(incoming_request_connection_id,
                                           transport::serialize_container(response_container),
                                           transport::MessageFormat::binary);
                }};
            auto catch_all_handler{[this](auto&&) {
                logger->error("Received unexpected request from Order Manager");
                logger->flush();
            }};

            std::visit(overloaded{new_order_handler, cancel_order_handler, fill_cost_query_handler,
                                  catch_all_handler},
                       container);

            auto snapshot = limit_order_book.get_top_order_book_level_aggregate();
            if (!shm_orderbook_snapshot.try_push(snapshot)) {
                std::cerr << "Failed to push snapshot " << "\n";
            }
        }
    }
}
} // namespace engine