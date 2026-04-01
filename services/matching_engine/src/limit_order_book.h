#pragma once

#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "order.h"
#include <limits>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>

namespace engine {

inline constexpr int MARKET_BID_ORDER_PRICE = std::numeric_limits<int>::max();
inline constexpr int MARKET_ASK_ORDER_PRICE = std::numeric_limits<int>::min();

using SideContainer = std::map<int, std::list<Order>>;

class LimitOrderBook {
  public:
    LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                   TradeRingBuffer shm_trade);
    LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container);

    [[nodiscard]] std::string_view get_ticker() const;

    void add_order(int order_id, int price, int quantity, Side side, std::string_view broker_id);
    void cancel_order(int order_id);

    [[nodiscard]] const SideContainer& get_side(Side side) const;

    [[nodiscard]] std::optional<std::reference_wrapper<const Order>>
    get_best_order(Side side) const;

    [[nodiscard]] LevelAggregate get_level_aggregate(Side side, int level) const;
    [[nodiscard]] TopOrderBookLevelAggregates get_top_order_book_level_aggregate() const;

    [[nodiscard]] bool order_id_exists(int order_id) const;
    [[nodiscard]] const Order& get_order_by_id(int order_id) const;

    [[nodiscard]] std::optional<int> get_fill_cost(int quantity, Side side) const;

  private:
    TradeRingBuffer shm_trade;

    std::queue<Trade>& trade_container;

    std::string ticker{};

    SideContainer bids{};
    SideContainer asks{};

    std::unordered_map<int, std::list<Order>::const_iterator> order_id_map{};

    void match_order(SideContainer& near_side, SideContainer& far_side, int price,
                     int remaining_quantity, int order_id, Side side, std::string_view broker_id);

    [[nodiscard]] SideContainer& get_side_mut(Side side);
    [[nodiscard]] Trade create_trade(int taker_order_id, int maker_order_id,
                                     std::string_view taker_id, std::string_view maker_id,
                                     int price, int quantity, Side taker_side) const;
};

[[nodiscard]] Trade create_trade(int taker_order_id, int maker_order_id, std::string_view taker_id,
                                 std::string_view maker_id, int price, int quantity,
                                 Side taker_side);

} // namespace engine
