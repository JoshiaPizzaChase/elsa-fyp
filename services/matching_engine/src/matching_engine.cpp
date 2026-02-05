#include "matching_engine.h"
#include "transport/messaging.h"

namespace engine {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

MatchingEngine::MatchingEngine(std::string_view host, int port)
    : logger{spdlog::basic_logger_mt<spdlog::async_factory>(
          "matching_engine_logger", std::string{PROJECT_ROOT_DIR} + "/logs/matching_engine.log")},
      inbound_ws_server{port, host, logger},
      ring_buffer{OrderbookSnapshotRingBuffer::open_exist_shm(
          core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE)},
      limit_order_book{"GME"}, latest_order_id{0} {
    inbound_ws_server.start();

    logger->info("Matching Engine constructed");
    logger->flush();
}

std::expected<void, std::string> MatchingEngine::start() {
    while (true) {
        if (auto new_message = inbound_ws_server.dequeue_message(0); new_message.has_value()) {
            const auto container = transport::deserialize_container(new_message.value());

            auto new_order_handler{[this](const core::NewOrderSingleContainer& new_order) {
                limit_order_book.add_order(
                    latest_order_id++,
                    new_order.price.value_or((new_order.side == core::Side::bid)
                                                 ? MARKET_BID_ORDER_PRICE
                                                 : MARKET_ASK_ORDER_PRICE),
                    new_order.order_qty,
                    (new_order.side == core::Side::bid) ? Side::Bid : Side::Ask);

                logger->info("New order request received");
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
            auto catch_all_handler{[this](auto&&) {
                logger->error("Received unexpected request from Order Manager");
                logger->flush();
            }};

            std::visit(overloaded{new_order_handler, cancel_order_handler, catch_all_handler},
                       container);

            auto snapshot = limit_order_book.get_top_order_book_level_aggregate();
            if (!ring_buffer.try_push(snapshot)) {
                std::cerr << "Failed to push snapshot " << "\n";
            }
        }
    }
}
} // namespace engine