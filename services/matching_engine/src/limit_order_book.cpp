#include "limit_order_book.h"

#include "core/constants.h"
#include <boost/contract.hpp>
#include <boost/uuid.hpp>
#include <chrono>
#include <queue>

namespace engine {
LimitOrderBook::LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                               std::unique_ptr<Publisher<Trade>> trade_publisher)
    : trade_publisher{std::move(trade_publisher)}, trade_events{trade_container},
      ticker{ticker} {
}

std::string_view LimitOrderBook::get_ticker() const {
    return ticker;
}

void LimitOrderBook::add_order(int order_id, int price, int quantity, Side side,
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

void LimitOrderBook::match_order(SideContainer& near_side, SideContainer& far_side, int price,
                                 int remaining_quantity, int order_id, Side side,
                                 std::string_view broker_id) {
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

            if (const auto order_quantity = front_order.get_quantity();
                remaining_quantity >= order_quantity) {
                remaining_quantity -= order_quantity;
                Trade new_trade = create_trade(order_id, front_order.get_order_id(), broker_id,
                                               front_order.get_trader_id(), ticker,
                                               front_order.get_price(), order_quantity, side);
                trade_publisher->try_publish(new_trade);
                trade_events.emplace(new_trade);

                std::cout << "New trade: " << new_trade << std::endl;

                order_id_map.erase(best_level_orders.front().get_order_id());
                best_level_orders.pop_front();
            } else {
                front_order.fill(remaining_quantity);

                Trade new_trade = create_trade(order_id, front_order.get_order_id(), broker_id,
                                               front_order.get_trader_id(), ticker,
                                               front_order.get_price(), remaining_quantity, side);
                trade_publisher->try_publish(new_trade);
                trade_events.emplace(new_trade);
                remaining_quantity = 0;
            }
        }

        if (best_level_orders.empty()) {
            far_side.erase(best_level_price);
        }
    }

    if (remaining_quantity > 0 && price != MARKET_BID_ORDER_PRICE &&
        price != MARKET_ASK_ORDER_PRICE) {
        std::cout << "In order object creation, trader_id: " << broker_id << std::endl;
        order_id_map[order_id] = near_side[price].emplace(near_side[price].end(), order_id, price,
                                                          remaining_quantity, side, broker_id);
    }
}

void LimitOrderBook::cancel_order(int order_id) {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });

    const auto order_it = order_id_map.find(order_id)->second;
    const int price = order_it->get_price();
    const Side side = order_it->get_side();

    auto& side_map = get_side_mut(side);
    auto& orders_at_price = side_map.at(price);
    orders_at_price.erase(order_it);

    if (orders_at_price.empty()) {
        side_map.erase(price);
    }

    order_id_map.erase(order_id);
}

std::optional<std::reference_wrapper<const Order>> LimitOrderBook::get_best_order(Side side) const {
    const auto& side_map = get_side(side);
    if (side_map.empty()) {
        return std::nullopt;
    }

    return (side == Side::bid) ? side_map.rbegin()->second.front()
                               : side_map.begin()->second.front();
}

bool LimitOrderBook::order_id_exists(int order_id) const {
    return order_id_map.contains(order_id);
}

const Order& LimitOrderBook::get_order_by_id(int order_id) const {
    boost::contract::check c = boost::contract::public_function(this).precondition(
        [&] { BOOST_CONTRACT_ASSERT(order_id_map.contains(order_id)); });

    return *(order_id_map.find(order_id)->second);
}

const SideContainer& LimitOrderBook::get_side(Side side) const {
    return (side == Side::bid) ? bids : asks;
}

SideContainer& LimitOrderBook::get_side_mut(Side side) {
    return (side == Side::bid) ? bids : asks;
}

LevelAggregate LimitOrderBook::get_level_aggregate(Side side, int level) const {
    LevelAggregate level_aggregate{};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] {
                                       const auto& side_map = get_side(side);
                                       BOOST_CONTRACT_ASSERT(!get_side(side).empty());
                                       BOOST_CONTRACT_ASSERT(level >= 0 && level < side_map.size());
                                   })
                                   .postcondition([&] {
                                       BOOST_CONTRACT_ASSERT(level_aggregate.price > 0);
                                       BOOST_CONTRACT_ASSERT(level_aggregate.quantity > 0);
                                   });

    const auto& side_map = get_side(side);
    const auto it = (side == Side::bid) ? std::prev(side_map.cend(), level + 1)
                                        : std::next(side_map.cbegin(), level);

    const int level_price = it->first;

    int level_quantity = 0;
    for (const auto& order : it->second) {
        level_quantity += order.get_quantity();
    }

    return level_aggregate = LevelAggregate{.price = level_price, .quantity = level_quantity};
}

TopOrderBookLevelAggregates LimitOrderBook::get_top_order_book_level_aggregate() const {
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

Trade create_trade(int taker_order_id, int maker_order_id, std::string_view taker_id,
                   std::string_view maker_id, std::string_view ticker, int price, int quantity,
                   Side taker_side) {
    boost::contract::check c = boost::contract::function().precondition([&] {
        BOOST_CONTRACT_ASSERT(taker_order_id >= 0);
        BOOST_CONTRACT_ASSERT(maker_order_id >= 0);
        BOOST_CONTRACT_ASSERT(taker_order_id != maker_order_id);
        BOOST_CONTRACT_ASSERT(!taker_id.empty());
        BOOST_CONTRACT_ASSERT(!maker_id.empty());
        BOOST_CONTRACT_ASSERT(!ticker.empty());
        BOOST_CONTRACT_ASSERT(price > 0);
        BOOST_CONTRACT_ASSERT(quantity > 0);
    });

    uint64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

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

// Calculates the total cost of filling required quantity starting from best orders. If there is
// insufficient liquidity, returns the total cost of fillable quantity.
std::optional<int> LimitOrderBook::get_fill_cost(int quantity, Side side) const {
    std::optional<int> total_cost{std::nullopt};
    boost::contract::check c = boost::contract::public_function(this)
                                   .precondition([&] { BOOST_CONTRACT_ASSERT(quantity > 0); })
                                   .postcondition([&] {
                                       std::ignore = total_cost.transform([](int c) -> int {
                                           BOOST_CONTRACT_ASSERT(c > 0);
                                           return c;
                                       });
                                   });

    const auto& side_map = get_side(side);
    if (side_map.empty()) {
        return std::nullopt;
    }

    total_cost = 0;
    for (int i{0}; i < side_map.size(); i++) {
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
