#pragma once

#include "core/orderbook_snapshot.h"
#include "core/trade.h"
#include "order.h"
#include "publisher.h"

#include <limits>
#include <list>
#include <map>
#include <memory_resource>
#include <queue>
#include <string>
#include <unordered_map>
#include <array>
#include <cstddef>

namespace engine {

inline constexpr int MARKET_BID_ORDER_PRICE = std::numeric_limits<int>::max();
inline constexpr int MARKET_ASK_ORDER_PRICE = std::numeric_limits<int>::min();

class LimitOrderBookMemoryPool {
  public:
    LimitOrderBookMemoryPool();
    [[nodiscard]] std::pmr::memory_resource* resource() noexcept;

  private:
    static constexpr std::size_t INITIAL_BUFFER_SIZE = 8 * 1024 * 1024;

    std::array<std::byte, INITIAL_BUFFER_SIZE> initial_buffer{};
    std::pmr::monotonic_buffer_resource monotonic_resource;
};

using OrderList = std::pmr::list<Order>;
using SideContainer = std::pmr::map<int, OrderList>;
using OrderIdMap = std::pmr::unordered_map<int, OrderList::const_iterator>;

class LimitOrderBook {
  public:
    LimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                   std::unique_ptr<Publisher<Trade>> trade_publisher);

    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;

    LimitOrderBook(LimitOrderBook&&) = default;
    LimitOrderBook& operator=(LimitOrderBook&&) = delete;

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
    std::unique_ptr<Publisher<Trade>> trade_publisher;

    std::queue<Trade>& trade_events;

    std::string ticker{};

    std::unique_ptr<LimitOrderBookMemoryPool> memory_pool{};
    SideContainer bids{};
    SideContainer asks{};

    OrderIdMap order_id_map{};

    void match_order(SideContainer& near_side, SideContainer& far_side, int price,
                     int remaining_quantity, int order_id, Side side, std::string_view broker_id);

    [[nodiscard]] SideContainer& get_side_mut(Side side);
};

[[nodiscard]] Trade create_trade(int taker_order_id, int maker_order_id, std::string_view taker_id,
                                 std::string_view maker_id, std::string_view ticker, int price,
                                 int quantity, Side taker_side);

} // namespace engine
