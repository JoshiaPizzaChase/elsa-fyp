#pragma once

#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "order.h"
#include <queue>
#include <expected>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <queue>

namespace engine {

constexpr int MARKET_BID_ORDER_PRICE = std::numeric_limits<int>::max();
constexpr int MARKET_ASK_ORDER_PRICE = std::numeric_limits<int>::min();

class LimitOrderBook {
  public:
    LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                   TradeRingBuffer shm_trade);
    explicit LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container);

    [[nodiscard]] std::string_view get_ticker() const;

    std::expected<void, std::string> add_order(int order_id, int price, int quantity, Side side,
                                               std::string_view trader_id);
    std::expected<void, std::string> cancel_order(int order_id);

    [[nodiscard]] const std::map<int, std::list<Order>>& get_side(Side side) const;

    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string>
    get_best_order(Side side) const;

    [[nodiscard]] std::expected<LevelAggregate, std::string> get_level_aggregate(Side side,
                                                                                 int level) const;
    [[nodiscard]] TopOrderBookLevelAggregates get_top_order_book_level_aggregate() const;

    [[nodiscard]] bool has_order_id(int order_id) const;

    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string>
    get_order_by_id(int order_id) const;

    [[nodiscard]] std::expected<int, std::string> get_fill_cost(int quantity, Side side) const;

  private:
    TradeRingBuffer shm_trade;

    std::queue<Trade>& trade_container;

    std::string_view ticker{};

    std::map<int, std::list<Order>> bids{};
    std::map<int, std::list<Order>> asks{};

    std::unordered_map<int, std::list<Order>::const_iterator> order_id_map{};

    void match_order(std::map<int, std::list<Order>>& near_side,
                     std::map<int, std::list<Order>>& far_side, int price, int remaining_quantity,
                     int order_id, Side side, std::string_view trader_id);
    [[nodiscard]] std::expected<Trade, std::string>
    create_trade(int taker_order_id, int maker_order_id, std::string_view taker_id,
                 std::string_view maker_id, int price, int quantity, Side taker_side);

    [[nodiscard]] std::map<int, std::list<Order>>& get_side_mut(Side side);
};

} // namespace engine
