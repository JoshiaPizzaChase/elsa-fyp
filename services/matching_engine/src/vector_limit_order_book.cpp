#include "vector_limit_order_book.h"

#include "core/constants.h"
#include <algorithm>
#include <boost/contract.hpp>
#include <chrono>
#include <cstddef>

namespace engine {
namespace {
constexpr bool is_bid_side(const Side side) {
    return side == Side::bid;
}
} // namespace

VectorLimitOrderBook::VectorLimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                                           std::unique_ptr<Publisher<Trade>> trade_publisher)
    : trade_publisher{std::move(trade_publisher)}, trade_events{trade_container}, ticker{ticker} {
}

std::string_view VectorLimitOrderBook::get_ticker() const {
    return ticker;
}

void VectorLimitOrderBook::add_order(int order_id, int price, int quantity, Side side,
                                     std::string_view broker_id) {
    boost::contract::check c = boost::contract::public_function(this).precondition([&] {
        BOOST_CONTRACT_ASSERT(order_id >= 0);
        BOOST_CONTRACT_ASSERT(!order_id_map.contains(order_id));
        BOOST_CONTRACT_ASSERT(price == MARKET_ASK_ORDER_PRICE || price > 0);
        BOOST_CONTRACT_ASSERT(quantity > 0);
        BOOST_CONTRACT_ASSERT(!broker_id.empty());
    });

    if (side == Side::bid) {
        match_order(bids, asks, price, quantity, order_id, side, broker_id);
    } else {
        match_order(asks, bids, price, quantity, order_id, side, broker_id);
    }
}

void VectorLimitOrderBook::match_order(SideLevels& near_side, SideLevels& far_side, int price,
                                       int remaining_quantity, int order_id, Side side,
                                       std::string_view broker_id) {
    while (!far_side.empty() && remaining_quantity > 0) {
        const std::size_t best_level_index = far_side.size() - 1;
        const int best_level_price = far_side.at(best_level_index).price;

        if ((side == Side::bid && price < best_level_price) ||
            (side == Side::ask && price > best_level_price)) {
            break;
        }

        auto& best_level = far_side.at(best_level_index);
        while (remaining_quantity > 0 && level_has_active_orders(best_level)) {
            const std::size_t active_order_index = first_active_order_index(best_level);
            auto& front_order = best_level.orders.at(active_order_index);
            const int front_order_id = front_order.get_order_id();

            if (const auto order_quantity = front_order.get_quantity();
                remaining_quantity >= order_quantity) {
                remaining_quantity -= order_quantity;
                Trade new_trade = create_trade(order_id, front_order_id, broker_id,
                                               front_order.get_trader_id(), ticker,
                                               front_order.get_price(), order_quantity, side);
                trade_publisher->try_publish(new_trade);
                trade_events.emplace(std::move(new_trade));

                order_id_map.erase(front_order_id);
                front_order.fill(order_quantity);
                best_level.first_active_index = active_order_index + 1;
            } else {
                front_order.fill(remaining_quantity);

                Trade new_trade = create_trade(order_id, front_order_id, broker_id,
                                               front_order.get_trader_id(), ticker,
                                               front_order.get_price(), remaining_quantity, side);
                trade_publisher->try_publish(new_trade);
                trade_events.emplace(std::move(new_trade));
                remaining_quantity = 0;
            }
        }

        if (!level_has_active_orders(best_level)) {
            far_side.pop_back();
        }
    }

    if (remaining_quantity > 0 && price != MARKET_BID_ORDER_PRICE && price != MARKET_ASK_ORDER_PRICE) {
        const std::size_t level_index = upsert_level(near_side, side, price);
        auto& resting_orders = near_side.at(level_index).orders;
        resting_orders.emplace_back(order_id, price, remaining_quantity, side, broker_id);
        order_id_map[order_id] = OrderLocation{side, price, resting_orders.size() - 1};
    }
}

void VectorLimitOrderBook::cancel_order(int order_id) {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });

    const auto order_location = order_id_map.find(order_id)->second;
    auto& side_levels = get_side_mut(order_location.side);
    const auto level_index = find_level_index(side_levels, order_location.side, order_location.price);
    BOOST_CONTRACT_ASSERT(level_index.has_value());
    auto& level = side_levels.at(level_index.value());
    auto& order = level.orders.at(order_location.order_index);
    order.fill(order.get_quantity());
    if (order_location.order_index == level.first_active_index) {
        level.first_active_index = first_active_order_index(level);
    }

    if (!level_has_active_orders(level)) {
        side_levels.erase(side_levels.begin() + static_cast<std::ptrdiff_t>(level_index.value()));
    }

    order_id_map.erase(order_id);
}

std::optional<std::reference_wrapper<const Order>> VectorLimitOrderBook::get_best_order(
    Side side) const {
    const auto& side_levels = get_side_levels(side);
    if (side_levels.empty()) {
        return std::nullopt;
    }

    const auto& best_level = side_levels.back();
    return std::cref(best_level.orders.at(first_active_order_index(best_level)));
}

bool VectorLimitOrderBook::order_id_exists(int order_id) const {
    return order_id_map.contains(order_id);
}

const Order& VectorLimitOrderBook::get_order_by_id(int order_id) const {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });

    const auto location = order_id_map.find(order_id)->second;
    const auto& side_levels = get_side_levels(location.side);
    const auto level_index = find_level_index(side_levels, location.side, location.price);
    BOOST_CONTRACT_ASSERT(level_index.has_value());
    return side_levels.at(level_index.value()).orders.at(location.order_index);
}

const SideContainer& VectorLimitOrderBook::get_side(Side side) const {
    if (side == Side::bid) {
        rebuild_side_cache(bids, bid_side_cache);
        return bid_side_cache;
    }

    rebuild_side_cache(asks, ask_side_cache);
    return ask_side_cache;
}

VectorLimitOrderBook::SideLevels& VectorLimitOrderBook::get_side_mut(Side side) {
    return is_bid_side(side) ? bids : asks;
}

const VectorLimitOrderBook::SideLevels& VectorLimitOrderBook::get_side_levels(Side side) const {
    return is_bid_side(side) ? bids : asks;
}

std::size_t VectorLimitOrderBook::upsert_level(SideLevels& side_levels, Side side, int price) {
    auto by_price_worst_to_best = [side](const PriceLevel& lhs, const int rhs_price) {
        if (is_bid_side(side)) {
            return lhs.price < rhs_price;
        }
        return lhs.price > rhs_price;
    };

    const auto it = std::lower_bound(side_levels.begin(), side_levels.end(), price, by_price_worst_to_best);
    if (it != side_levels.end() && it->price == price) {
        return static_cast<std::size_t>(std::distance(side_levels.begin(), it));
    }

    const auto insert_it = side_levels.emplace(it, PriceLevel{price, {}, 0});
    return static_cast<std::size_t>(std::distance(side_levels.begin(), insert_it));
}

std::optional<std::size_t> VectorLimitOrderBook::find_level_index(const SideLevels& side_levels, Side side,
                                                                   int price) const {
    auto by_price_worst_to_best = [side](const PriceLevel& lhs, const int rhs_price) {
        if (is_bid_side(side)) {
            return lhs.price < rhs_price;
        }
        return lhs.price > rhs_price;
    };

    const auto it = std::lower_bound(side_levels.begin(), side_levels.end(), price, by_price_worst_to_best);
    if (it == side_levels.end() || it->price != price) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(side_levels.begin(), it));
}

bool VectorLimitOrderBook::level_has_active_orders(const PriceLevel& level) {
    return first_active_order_index(level) < level.orders.size();
}

std::size_t VectorLimitOrderBook::first_active_order_index(const PriceLevel& level) {
    std::size_t index = level.first_active_index;
    while (index < level.orders.size() && level.orders.at(index).get_quantity() <= 0) {
        ++index;
    }
    return index;
}

void VectorLimitOrderBook::rebuild_side_cache(const SideLevels& side_levels, SideContainer& cache) const {
    cache.clear();
    for (const auto& level : side_levels) {
        auto& level_orders = cache[level.price];
        level_orders.clear();
        const auto active_start = first_active_order_index(level);
        for (std::size_t order_index = active_start; order_index < level.orders.size(); ++order_index) {
            const auto& order = level.orders.at(order_index);
            if (order.get_quantity() > 0) {
                level_orders.emplace_back(order);
            }
        }
    }
}

LevelAggregate VectorLimitOrderBook::get_level_aggregate(Side side, int level) const {
    LevelAggregate level_aggregate{};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] {
                                       const auto& side_levels = get_side_levels(side);
                                       BOOST_CONTRACT_ASSERT(!side_levels.empty());
                                       BOOST_CONTRACT_ASSERT(level >= 0 &&
                                                             level < static_cast<int>(side_levels.size()));
                                   })
                                   .postcondition([&] {
                                       BOOST_CONTRACT_ASSERT(level_aggregate.price > 0);
                                       BOOST_CONTRACT_ASSERT(level_aggregate.quantity > 0);
                                   });

    const auto& side_levels = get_side_levels(side);
    const std::size_t level_index = side_levels.size() - 1 - static_cast<std::size_t>(level);
    const auto& level_data = side_levels.at(level_index);
    const auto& level_orders = level_data.orders;

    int level_quantity = 0;
    for (std::size_t order_index = first_active_order_index(level_data); order_index < level_orders.size();
         ++order_index) {
        level_quantity += level_orders.at(order_index).get_quantity();
    }

    return level_aggregate =
               LevelAggregate{.price = side_levels.at(level_index).price, .quantity = level_quantity};
}

TopOrderBookLevelAggregates VectorLimitOrderBook::get_top_order_book_level_aggregate() const {
    uint64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

    TopOrderBookLevelAggregates top_aggregate{ticker.data(), now_ts_ms};

    const auto bid_level_count = bids.size();
    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS && i < bid_level_count; i++) {
        top_aggregate.bid_level_aggregates.at(i) = get_level_aggregate(Side::bid, i);
    }

    const auto ask_level_count = asks.size();
    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS && i < ask_level_count; i++) {
        top_aggregate.ask_level_aggregates.at(i) = get_level_aggregate(Side::ask, i);
    }

    return top_aggregate;
}

std::optional<int> VectorLimitOrderBook::get_fill_cost(int quantity, Side side) const {
    std::optional<int> total_cost{std::nullopt};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] { BOOST_CONTRACT_ASSERT(quantity > 0); })
                                   .postcondition([&] {
                                       std::ignore = total_cost.transform([](int c) -> int {
                                           BOOST_CONTRACT_ASSERT(c > 0);
                                           return c;
                                       });
                                   });

    const auto& side_levels = get_side_levels(side);
    if (side_levels.empty()) {
        return std::nullopt;
    }

    total_cost = 0;
    for (int i{0}; i < static_cast<int>(side_levels.size()); i++) {
        if (quantity == 0) {
            break;
        }

        auto level_aggregate = get_level_aggregate(side, i);

        if (level_aggregate.quantity <= quantity) {
            total_cost.value() += level_aggregate.price * level_aggregate.quantity;
            quantity -= level_aggregate.quantity;
        } else {
            total_cost.value() += level_aggregate.price * quantity;
            quantity = 0;
        }
    }
    return total_cost;
}

} // namespace engine
