#ifndef ELSA_FYP_LIMIT_ORDER_BOOK_H
#define ELSA_FYP_LIMIT_ORDER_BOOK_H

#include "order.h"
#include <array>
#include <expected>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <unordered_map>

namespace engine {

constexpr int MARKET_BID_ORDER_PRICE = std::numeric_limits<int>::max();
constexpr int MARKET_ASK_ORDER_PRICE = std::numeric_limits<int>::min();

constexpr int ORDER_BOOK_AGGREGATE_LEVELS = 50;

struct LevelAggregate {
    int price{0};
    int quantity{0};
};

struct TopOrderBookLevelAggregates {
    std::array<LevelAggregate, ORDER_BOOK_AGGREGATE_LEVELS> bid_level_aggregates;
    std::array<LevelAggregate, ORDER_BOOK_AGGREGATE_LEVELS> ask_level_aggregates;
};

class LimitOrderBook {
  public:
    std::expected<void, std::string> add_order(int order_id, int price, int quantity, Side side);
    std::expected<void, std::string> cancel_order(int order_id);

    [[nodiscard]] const std::map<int, std::list<Order>>& get_side(Side side) const;

    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string>
    get_best_order(Side side) const;

    [[nodiscard]] std::expected<LevelAggregate, std::string> get_level_aggregate(Side side,
                                                                                 int level) const;
    [[nodiscard]] TopOrderBookLevelAggregates get_top_order_book_level_aggregate() const;

    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string>
    get_order_by_id(int order_id) const;

  private:
    std::map<int, std::list<Order>> bids{};
    std::map<int, std::list<Order>> asks{};

    std::unordered_map<int, std::list<Order>::const_iterator> order_id_map{};

    void match_order(std::map<int, std::list<Order>>& near_side,
                     std::map<int, std::list<Order>>& far_side, int price, int quantity,
                     int order_id, Side side);

    [[nodiscard]] std::map<int, std::list<Order>>& get_side_mut(Side side);
};

} // namespace engine

#endif // ELSA_FYP_LIMIT_ORDER_BOOK_H
