#pragma once

#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "order.h"
#include <limits>
#include <list>
#include <map>
#include <unordered_map>

namespace engine {

inline constexpr int MARKET_BID_ORDER_PRICE = std::numeric_limits<int>::max();
inline constexpr int MARKET_ASK_ORDER_PRICE = std::numeric_limits<int>::min();

using SideContainer = std::map<int, std::list<Order>>;

class LimitOrderBook {
  public:
    explicit LimitOrderBook(std::string_view ticker);

    [[nodiscard]] std::string_view get_ticker() const;

    void add_order(int order_id, int price, int quantity, Side side);
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
    OrderbookSnapshotRingBuffer shm_orderbook_snapshot;
    TradeRingBuffer shm_trade;

    std::string ticker{};

    SideContainer bids{};
    SideContainer asks{};

    std::unordered_map<int, std::list<Order>::const_iterator> order_id_map{};

    void match_order(SideContainer& near_side, SideContainer& far_side, int price,
                     int remaining_quantity, int order_id, Side side);

    [[nodiscard]] Trade create_trade(int taker_order_id, int maker_order_id, int price,
                                     int quantity, Side taker_side) const;

    [[nodiscard]] SideContainer& get_side_mut(Side side);
};

} // namespace engine
