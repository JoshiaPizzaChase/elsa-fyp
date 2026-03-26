#include "limit_order_book.h"
#include "core/constants.h"
#include <boost/uuid.hpp>
#include <chrono>
#include <expected>
#include <format>
#include <queue>

namespace engine {
LimitOrderBook::LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container, TradeRingBuffer shm_trade)
    : ticker{ticker}, shm_trade{std::move(shm_trade)}, trade_container{trade_container} {
}

LimitOrderBook::LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container)
    : ticker{ticker}, shm_trade{TradeRingBuffer::create("dummy", true)}, trade_container{trade_container} {
}

std::string_view LimitOrderBook::get_ticker() const {
    return ticker;
}

std::expected<void, std::string> LimitOrderBook::add_order(int order_id, int price, int quantity,
                                                           Side side, std::string_view trader_id) {
    if (order_id_map.contains(order_id)) {
        return std::unexpected(std::format("Order ID {} already exists in order book", order_id));
    }

    if (price != MARKET_ASK_ORDER_PRICE && price <= 0) {
        return std::unexpected("Price must be positive integers or int min");
    }

    if (quantity <= 0) {
        return std::unexpected("Quantity must be positive integers");
    }

    if (side == Side::bid) {
        match_order(bids, asks, price, quantity, order_id, side, trader_id);
    } else {
        match_order(asks, bids, price, quantity, order_id, side, trader_id);
    }

    return {};
}

void LimitOrderBook::match_order(std::map<int, std::list<Order>>& near_side,
                                 std::map<int, std::list<Order>>& far_side, int price,
                                 int remaining_quantity, int order_id, Side side,
                                 std::string_view trader_id) {
    while (!far_side.empty() && remaining_quantity > 0) {
        const auto best_level = (side == Side::bid) ? far_side.begin() : std::prev(far_side.end());

        const auto& best_level_price = best_level->first;
        if ((side == Side::bid && price < best_level_price) ||
            (side == Side::ask && price > best_level_price)) {
            break;
        }

        auto& best_level_orders = best_level->second;
        while (remaining_quantity > 0 && !best_level_orders.empty()) {
            auto& front_order = best_level_orders.front();

            const int matched_price =
                (price == MARKET_BID_ORDER_PRICE || price == MARKET_ASK_ORDER_PRICE)
                    ? front_order.get_price()
                    : price;

            if (const auto order_quantity = front_order.get_quantity();
                remaining_quantity >= order_quantity) {
                remaining_quantity -= order_quantity;
                Trade new_trade =
                    create_trade(order_id, front_order.get_order_id(), trader_id,
                                 front_order.get_trader_id(), matched_price, order_quantity, side)
                        .value();
                shm_trade.try_push(new_trade);
                trade_container.emplace(new_trade);

                std::cout << "New trade: " << new_trade << std::endl;

                order_id_map.erase(best_level_orders.front().get_order_id());
                best_level_orders.pop_front();
            } else {
                front_order.fill(remaining_quantity);

                Trade new_trade =
                    create_trade(order_id, front_order.get_order_id(), trader_id,
                                 front_order.get_trader_id(), price, remaining_quantity, side)
                        .value();
                shm_trade.try_push(new_trade);
                trade_container.emplace(new_trade);
                remaining_quantity = 0;
            }
        }

        if (best_level_orders.empty()) {
            far_side.erase(best_level_price);
        }
    }

    if (remaining_quantity > 0 && price != MARKET_BID_ORDER_PRICE &&
        price != MARKET_ASK_ORDER_PRICE) {
        std::cout << "In order object creation, trader_id: " << trader_id << std::endl;
        order_id_map[order_id] = near_side[price].emplace(near_side[price].end(), order_id, price,
                                                          remaining_quantity, side, trader_id);
    }
}

std::expected<void, std::string> LimitOrderBook::cancel_order(int order_id) {
    const auto it = order_id_map.find(order_id);
    if (it == order_id_map.end()) {
        return std::unexpected(std::format("Order ID {} not found in order book", order_id));
    }

    const auto& order_it = it->second;
    const int price = order_it->get_price();
    const Side side = order_it->get_side();

    auto& side_map = get_side_mut(side);
    auto& orders_at_price = side_map[price];
    orders_at_price.erase(order_it);

    if (orders_at_price.empty()) {
        side_map.erase(price);
    }

    order_id_map.erase(order_id);

    return {};
}

std::expected<std::reference_wrapper<const Order>, std::string>
LimitOrderBook::get_best_order(Side side) const {
    const auto& side_map = get_side(side);
    if (side_map.empty()) {
        return std::unexpected(
            std::format("No {} orders in order book", (side == Side::bid) ? "bid" : "ask"));
    }

    return (side == Side::bid) ? side_map.rbegin()->second.front()
                               : side_map.begin()->second.front();
}

bool LimitOrderBook::has_order_id(int order_id) const {
    return order_id_map.contains(order_id);
}

std::expected<std::reference_wrapper<const Order>, std::string>
LimitOrderBook::get_order_by_id(int order_id) const {
    const auto iter = order_id_map.find(order_id);
    if (iter == order_id_map.end()) {
        return std::unexpected(std::format("Order ID {} not found in order book", order_id));
    }
    return *(iter->second);
}

const std::map<int, std::list<Order>>& LimitOrderBook::get_side(Side side) const {
    return (side == Side::bid) ? bids : asks;
}

std::map<int, std::list<Order>>& LimitOrderBook::get_side_mut(Side side) {
    return (side == Side::bid) ? bids : asks;
}

std::expected<LevelAggregate, std::string> LimitOrderBook::get_level_aggregate(Side side,
                                                                               int level) const {
    const auto& side_map = get_side(side);
    if (side_map.empty()) {
        return std::unexpected(
            std::format("No {} orders in order book", (side == Side::bid) ? "bid" : "ask"));
    }

    if (level >= side_map.size()) {
        return std::unexpected(std::format("Level {} does not exist in {} side", level,
                                           (side == Side::bid) ? "bid" : "ask"));
    }

    const auto it = (side == Side::bid) ? std::prev(side_map.cend(), level + 1)
                                        : std::next(side_map.cbegin(), level);

    const int level_price = it->first;

    int level_quantity = 0;
    for (const auto& order : it->second) {
        level_quantity += order.get_quantity();
    }

    return LevelAggregate{.price = level_price, .quantity = level_quantity};
}

TopOrderBookLevelAggregates LimitOrderBook::get_top_order_book_level_aggregate() const {
    uint64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

    TopOrderBookLevelAggregates top_aggregate{ticker.data(), now_ts_ms};

    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; i++) {
        if (const auto level_aggregate = get_level_aggregate(Side::bid, i);
            level_aggregate.has_value()) {
            top_aggregate.bid_level_aggregates.at(i) = level_aggregate.value();
        } else {
            break;
        }
    }

    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; i++) {
        if (const auto level_aggregate = get_level_aggregate(Side::ask, i);
            level_aggregate.has_value()) {
            top_aggregate.ask_level_aggregates.at(i) = level_aggregate.value();
        } else {
            break;
        }
    }

    return top_aggregate;
}

std::expected<Trade, std::string>
LimitOrderBook::create_trade(int taker_order_id, int maker_order_id, std::string_view taker_id,
                             std::string_view maker_id, int price, int quantity, Side taker_side) {
    if (price <= 0) {
        return std::unexpected("Price must be positive integers");
    }

    if (quantity <= 0) {
        return std::unexpected("Quantity must be positive integers");
    }

    uint64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

    // TODO: Add random trade_id generation
    // TODO: add back taker and maker id
    return Trade{ticker.data(),
                 price,
                 quantity,
                 boost::uuids::to_string(boost::uuids::time_generator_v7()()).data(),
                 taker_id.data(),
                 maker_id.data(),
                 taker_order_id,
                 maker_order_id,
                 taker_side == Side::bid,
                 now_ts_ms};
}

std::expected<int, std::string> LimitOrderBook::get_fill_cost(int quantity, Side side) const {
    if (quantity <= 0) {
        return std::unexpected("Quantity must be positive integer");
    }

    const auto far_side = (side == Side::bid) ? Side::ask : Side::bid;
    const auto& far_side_map = get_side(far_side);
    if (far_side_map.empty()) {
        return std::unexpected(
            std::format("No {} orders in order book", (side == Side::bid) ? "bid" : "ask"));
    }

    int total_cost{0};

    for (int i{0}; i < far_side_map.size(); i++) {
        if (quantity == 0) {
            break;
        }

        auto level_aggregate = get_level_aggregate(far_side, i).value();

        if (level_aggregate.quantity <= quantity) {
            total_cost += level_aggregate.price * level_aggregate.quantity;
            quantity -= level_aggregate.quantity;
        } else {
            total_cost += level_aggregate.price * quantity;
            quantity = 0;
        }
    }

    return total_cost;
}
} // namespace engine
