#include "flat_vector_limit_order_book.h"

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

FlatVectorLimitOrderBook::FlatVectorLimitOrderBook(std::string_view ticker,
                                                   std::queue<Trade>& trade_container,
                                                   std::unique_ptr<Publisher<Trade>> trade_publisher)
    : trade_publisher{std::move(trade_publisher)}, trade_events{trade_container}, ticker{ticker} {
}

std::string_view FlatVectorLimitOrderBook::get_ticker() const {
    return ticker;
}

void FlatVectorLimitOrderBook::add_order(int order_id, int price, int quantity, Side side,
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

void FlatVectorLimitOrderBook::match_order(SideStorage& near_side, SideStorage& far_side, int price,
                                           int remaining_quantity, int order_id, Side side,
                                           std::string_view broker_id) {
    while (!far_side.levels.empty() && remaining_quantity > 0) {
        auto& best_level = far_side.levels.back();
        const int best_level_price = best_level.price;
        if ((side == Side::bid && price < best_level_price) ||
            (side == Side::ask && price > best_level_price)) {
            break;
        }

        while (remaining_quantity > 0 && has_active_orders(far_side, best_level)) {
            const std::size_t maker_index = first_active_order_index(far_side, best_level);
            auto& maker = *far_side.orders.at(maker_index);
            const int maker_order_id = maker.get_order_id();
            const int maker_quantity = maker.get_quantity();

            if (remaining_quantity >= maker_quantity) {
                remaining_quantity -= maker_quantity;
                Trade trade = create_trade(order_id, maker_order_id, broker_id, maker.get_trader_id(), ticker,
                                           maker.get_price(), maker_quantity, side);
                trade_publisher->try_publish(trade);
                trade_events.emplace(std::move(trade));

                order_id_map.erase(maker_order_id);
                maker.fill(maker_quantity);
                best_level.first_active = maker_index + 1;
            } else {
                maker.fill(remaining_quantity);
                Trade trade = create_trade(order_id, maker_order_id, broker_id, maker.get_trader_id(), ticker,
                                           maker.get_price(), remaining_quantity, side);
                trade_publisher->try_publish(trade);
                trade_events.emplace(std::move(trade));
                remaining_quantity = 0;
            }
        }

        if (!has_active_orders(far_side, best_level)) {
            far_side.levels.pop_back();
        }
    }

    if (remaining_quantity > 0 && price != MARKET_BID_ORDER_PRICE && price != MARKET_ASK_ORDER_PRICE) {
        const std::size_t level_index = upsert_level(near_side, side, price);
        auto& level = near_side.levels.at(level_index);
        const std::size_t insert_pos = level.end;
        auto order_ptr = std::make_unique<Order>(order_id, price, remaining_quantity, side, broker_id);
        Order* const raw_order = order_ptr.get();
        near_side.orders.insert(near_side.orders.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                                std::move(order_ptr));
        shift_levels_after_insert(near_side.levels, level_index, insert_pos);
        order_id_map[order_id] = OrderLocation{side, price, raw_order};
    }
}

void FlatVectorLimitOrderBook::cancel_order(int order_id) {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });

    const auto location = order_id_map.find(order_id)->second;
    auto& side_storage = get_side_mut(location.side);
    const auto level_index = find_level_index(side_storage, location.side, location.price);
    BOOST_CONTRACT_ASSERT(level_index.has_value());

    auto& level = side_storage.levels.at(level_index.value());
    std::size_t found_index = level.end;
    for (std::size_t i = level.begin; i < level.end; ++i) {
        if (side_storage.orders.at(i).get() == location.order) {
            found_index = i;
            break;
        }
    }
    BOOST_CONTRACT_ASSERT(found_index < level.end);

    auto& order = *side_storage.orders.at(found_index);
    order.fill(order.get_quantity());
    if (found_index == level.first_active) {
        level.first_active = first_active_order_index(side_storage, level);
    }
    if (!has_active_orders(side_storage, level)) {
        side_storage.levels.erase(side_storage.levels.begin() +
                                  static_cast<std::ptrdiff_t>(level_index.value()));
    }

    order_id_map.erase(order_id);
}

const SideContainer& FlatVectorLimitOrderBook::get_side(Side side) const {
    if (side == Side::bid) {
        rebuild_side_cache(bids, bid_side_cache);
        return bid_side_cache;
    }

    rebuild_side_cache(asks, ask_side_cache);
    return ask_side_cache;
}

std::optional<std::reference_wrapper<const Order>> FlatVectorLimitOrderBook::get_best_order(Side side) const {
    const auto& storage = get_side_storage(side);
    for (auto it = storage.levels.rbegin(); it != storage.levels.rend(); ++it) {
        const auto idx = first_active_order_index(storage, *it);
        if (idx < it->end) {
            return std::cref(*storage.orders.at(idx));
        }
    }
    return std::nullopt;
}

LevelAggregate FlatVectorLimitOrderBook::get_level_aggregate(Side side, int level) const {
    LevelAggregate level_aggregate{};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] {
                                       const auto& side_levels = get_side_storage(side).levels;
                                       BOOST_CONTRACT_ASSERT(!side_levels.empty());
                                       BOOST_CONTRACT_ASSERT(level >= 0 &&
                                                             level < static_cast<int>(side_levels.size()));
                                   })
                                   .postcondition([&] {
                                       BOOST_CONTRACT_ASSERT(level_aggregate.price > 0);
                                       BOOST_CONTRACT_ASSERT(level_aggregate.quantity > 0);
                                   });

    const auto& side_storage = get_side_storage(side);
    const std::size_t level_index = side_storage.levels.size() - 1 - static_cast<std::size_t>(level);
    const auto& level_data = side_storage.levels.at(level_index);
    int level_quantity = 0;
    const std::size_t active_start = first_active_order_index(side_storage, level_data);
    for (std::size_t i = active_start; i < level_data.end; ++i) {
        level_quantity += side_storage.orders.at(i)->get_quantity();
    }

    return level_aggregate = LevelAggregate{.price = level_data.price, .quantity = level_quantity};
}

TopOrderBookLevelAggregates FlatVectorLimitOrderBook::get_top_order_book_level_aggregate() const {
    uint64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

    TopOrderBookLevelAggregates top_aggregate{ticker.data(), now_ts_ms};

    const auto bid_level_count = bids.levels.size();
    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS && i < bid_level_count; i++) {
        top_aggregate.bid_level_aggregates.at(i) = get_level_aggregate(Side::bid, i);
    }

    const auto ask_level_count = asks.levels.size();
    for (int i{0}; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS && i < ask_level_count; i++) {
        top_aggregate.ask_level_aggregates.at(i) = get_level_aggregate(Side::ask, i);
    }

    return top_aggregate;
}

bool FlatVectorLimitOrderBook::order_id_exists(int order_id) const {
    return order_id_map.contains(order_id);
}

const Order& FlatVectorLimitOrderBook::get_order_by_id(int order_id) const {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });
    return *(order_id_map.find(order_id)->second.order);
}

std::optional<int> FlatVectorLimitOrderBook::get_fill_cost(int quantity, Side side) const {
    std::optional<int> total_cost{std::nullopt};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] { BOOST_CONTRACT_ASSERT(quantity > 0); })
                                   .postcondition([&] {
                                       std::ignore = total_cost.transform([](int c) -> int {
                                           BOOST_CONTRACT_ASSERT(c > 0);
                                           return c;
                                       });
                                   });

    const auto& side_levels = get_side_storage(side).levels;
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

FlatVectorLimitOrderBook::SideStorage& FlatVectorLimitOrderBook::get_side_mut(Side side) {
    return is_bid_side(side) ? bids : asks;
}

const FlatVectorLimitOrderBook::SideStorage& FlatVectorLimitOrderBook::get_side_storage(Side side) const {
    return is_bid_side(side) ? bids : asks;
}

std::optional<std::size_t> FlatVectorLimitOrderBook::find_level_index(const SideStorage& side_storage,
                                                                       Side side, int price) const {
    auto by_price_worst_to_best = [side](const PriceLevel& lhs, const int rhs_price) {
        if (is_bid_side(side)) {
            return lhs.price < rhs_price;
        }
        return lhs.price > rhs_price;
    };

    const auto it =
        std::lower_bound(side_storage.levels.begin(), side_storage.levels.end(), price,
                         by_price_worst_to_best);
    if (it == side_storage.levels.end() || it->price != price) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(side_storage.levels.begin(), it));
}

std::size_t FlatVectorLimitOrderBook::upsert_level(SideStorage& side_storage, Side side, int price) {
    auto by_price_worst_to_best = [side](const PriceLevel& lhs, const int rhs_price) {
        if (is_bid_side(side)) {
            return lhs.price < rhs_price;
        }
        return lhs.price > rhs_price;
    };

    const auto it =
        std::lower_bound(side_storage.levels.begin(), side_storage.levels.end(), price,
                         by_price_worst_to_best);
    if (it != side_storage.levels.end() && it->price == price) {
        return static_cast<std::size_t>(std::distance(side_storage.levels.begin(), it));
    }

    const std::size_t insert_level_index = static_cast<std::size_t>(
        std::distance(side_storage.levels.begin(), it));
    const std::size_t insert_pos =
        insert_level_index == side_storage.levels.size()
            ? side_storage.orders.size()
            : side_storage.levels.at(insert_level_index).begin;
    side_storage.levels.insert(it, PriceLevel{price, insert_pos, insert_pos, insert_pos});
    return insert_level_index;
}

void FlatVectorLimitOrderBook::shift_levels_after_insert(std::vector<PriceLevel>& levels,
                                                          std::size_t start_level_index,
                                                          std::size_t insert_pos) {
    auto& level = levels.at(start_level_index);
    level.end += 1;
    if (level.first_active > insert_pos) {
        level.first_active += 1;
    }

    for (std::size_t i = start_level_index + 1; i < levels.size(); ++i) {
        levels.at(i).begin += 1;
        levels.at(i).end += 1;
        if (levels.at(i).first_active >= insert_pos) {
            levels.at(i).first_active += 1;
        }
    }
}

std::size_t FlatVectorLimitOrderBook::first_active_order_index(const SideStorage& side_storage,
                                                               const PriceLevel& level) {
    std::size_t index = level.first_active;
    while (index < level.end && side_storage.orders.at(index)->get_quantity() <= 0) {
        ++index;
    }
    return index;
}

bool FlatVectorLimitOrderBook::has_active_orders(const SideStorage& side_storage, const PriceLevel& level) {
    return first_active_order_index(side_storage, level) < level.end;
}

void FlatVectorLimitOrderBook::rebuild_side_cache(const SideStorage& side_storage,
                                                  SideContainer& cache) const {
    cache.clear();
    for (const auto& level : side_storage.levels) {
        auto& level_orders = cache[level.price];
        level_orders.clear();
        const auto active_start = first_active_order_index(side_storage, level);
        for (std::size_t i = active_start; i < level.end; ++i) {
            const auto& order = *side_storage.orders.at(i);
            if (order.get_quantity() > 0) {
                level_orders.emplace_back(order);
            }
        }
    }
}

} // namespace engine
