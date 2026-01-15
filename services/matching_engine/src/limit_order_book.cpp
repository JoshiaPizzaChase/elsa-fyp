#include "limit_order_book.h"
#include <expected>
#include <format>
#include <limits>

namespace engine {
std::expected<void, std::string> LimitOrderBook::add_order(int order_id, int price, int quantity,
                                                           Side side) {
    if (order_id_map.contains(order_id)) {
        return std::unexpected(std::format("Order ID {} already exists in order book", order_id));
    }

    if (price != std::numeric_limits<int>::min() && price <= 0) {
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
                                 std::map<int, std::list<Order>>& far_side, int price, int quantity,
                                 int order_id, Side side) {
    while (!far_side.empty() && quantity > 0) {
        const auto best_entry = (side == Side::Bid) ? std::prev(far_side.end()) : far_side.begin();

        const auto& best_entry_price = best_entry->first;
        if ((side == Side::Bid && price < best_entry_price) ||
            (side == Side::Ask && price > best_entry_price)) {
            break;
        }

        auto& best_entry_orders = best_entry->second;
        while (quantity > 0 && !best_entry_orders.empty()) {
            auto& front_order = best_entry_orders.front();
            if (const auto order_quantity = front_order.get_quantity();
                quantity >= order_quantity) {
                quantity -= order_quantity;
                order_id_map.erase(best_entry_orders.front().get_order_id());
                best_entry_orders.pop_front();
            } else {
                front_order.fill(quantity);
                quantity = 0;
            }
        }

        if (best_entry_orders.empty()) {
            far_side.erase(best_entry_price);
        }
    }

    if (quantity > 0) {
        order_id_map[order_id] =
            near_side[price].emplace(near_side[price].end(), order_id, price, quantity, side);
        ;
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

    auto& side_map = (side == Side::Bid) ? bids : asks;
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
    const auto& side_map = (side == Side::Bid) ? bids : asks;
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

} // namespace engine
