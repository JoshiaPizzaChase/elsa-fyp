#include "limit_order_book.h"
#include <expected>
#include <format>

namespace engine {
LimitOrderBook::LimitOrderBook(std::string_view ticker) : ticker{ticker} {
}

std::string_view LimitOrderBook::get_ticker() const {
    return ticker;
}

std::expected<void, std::string> LimitOrderBook::add_order(int order_id, int price, int quantity,
                                                           Side side) {
    if (order_id_map.contains(order_id)) {
        return std::unexpected(std::format("Order ID {} already exists in order book", order_id));
    }

    if (price != MARKET_ASK_ORDER_PRICE && price <= 0) {
        return std::unexpected("Price must be positive integers or int min");
    }

    if (quantity <= 0) {
        return std::unexpected("Quantity must be positive integers");
    }

    if (side == Side::Bid) {
        match_order(bids, asks, price, quantity, order_id, side);
    } else {
        match_order(asks, bids, price, quantity, order_id, side);
    }

    return {};
}

void LimitOrderBook::match_order(std::map<int, std::list<Order>>& near_side,
                                 std::map<int, std::list<Order>>& far_side, int price,
                                 int remaining_quantity, int order_id, Side side) {
    while (!far_side.empty() && remaining_quantity > 0) {
        const auto best_level = (side == Side::Bid) ? std::prev(far_side.end()) : far_side.begin();

        const auto& best_level_price = best_level->first;
        if ((side == Side::Bid && price < best_level_price) ||
            (side == Side::Ask && price > best_level_price)) {
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
                order_id_map.erase(best_level_orders.front().get_order_id());
                best_level_orders.pop_front();

                Trade new_trade = create_trade(order_id, front_order.get_order_id(), matched_price,
                                               order_quantity)
                                      .value();

            } else {
                front_order.fill(remaining_quantity);

                Trade new_trade =
                    create_trade(order_id, front_order.get_order_id(), price, remaining_quantity)
                        .value();

                remaining_quantity = 0;
            }
        }

        if (best_level_orders.empty()) {
            far_side.erase(best_level_price);
        }
    }

    if (remaining_quantity > 0) {
        order_id_map[order_id] = near_side[price].emplace(near_side[price].end(), order_id, price,
                                                          remaining_quantity, side);
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
            std::format("No {} orders in order book", (side == Side::Bid) ? "bid" : "ask"));
    }

    return (side == Side::Bid) ? side_map.rbegin()->second.front()
                               : side_map.begin()->second.front();
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
    return (side == Side::Bid) ? bids : asks;
}

std::map<int, std::list<Order>>& LimitOrderBook::get_side_mut(Side side) {
    return (side == Side::Bid) ? bids : asks;
}

std::expected<LevelAggregate, std::string> LimitOrderBook::get_level_aggregate(Side side,
                                                                               int level) const {
    const auto& side_map = get_side(side);
    if (side_map.empty()) {
        return std::unexpected(
            std::format("No {} orders in order book", (side == Side::Bid) ? "bid" : "ask"));
    }

    if (level >= side_map.size()) {
        return std::unexpected(std::format("Level {} does not exist in {} side", level,
                                           (side == Side::Bid) ? "bid" : "ask"));
    }

    const auto it = (side == Side::Bid) ? std::prev(side_map.cend(), level + 1)
                                        : std::next(side_map.cbegin(), level);

    const int level_price = it->first;

    int level_quantity = 0;
    for (const auto& order : it->second) {
        level_quantity += order.get_quantity();
    }

    return LevelAggregate{.price = level_price, .quantity = level_quantity};
}

TopOrderBookLevelAggregates LimitOrderBook::get_top_order_book_level_aggregate() const {
    TopOrderBookLevelAggregates top_aggregate{ticker.data()};

    for (int i = 0; i < bids.size(); i++) {
        if (const auto level_aggregate = get_level_aggregate(Side::Bid, i);
            level_aggregate.has_value()) {
            top_aggregate.bid_level_aggregates.at(i) = level_aggregate.value();
        } else {
            break;
        }
    }

    for (int i = 0; i < asks.size(); i++) {
        if (const auto level_aggregate = get_level_aggregate(Side::Ask, i);
            level_aggregate.has_value()) {
            top_aggregate.ask_level_aggregates.at(i) = level_aggregate.value();
        } else {
            break;
        }
    }

    return top_aggregate;
}

std::expected<Trade, std::string>
LimitOrderBook::create_trade(int taker_order_id, int maker_order_id, int price, int quantity) {
    if (price <= 0) {
        return std::unexpected("Price must be positive integers");
    }

    if (quantity <= 0) {
        return std::unexpected("Quantity must be positive integers");
    }

    // TODO: Add random trade_id generation

    return Trade{.taker_order_id = taker_order_id,
                 .maker_order_id = maker_order_id,
                 .price = price,
                 .quantity = quantity};
}
} // namespace engine
