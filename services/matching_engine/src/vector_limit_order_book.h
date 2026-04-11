#pragma once

#include "limit_order_book.h"

#include <vector>

namespace engine {

class VectorLimitOrderBook {
  public:
    VectorLimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                         std::unique_ptr<Publisher<Trade>> trade_publisher);

    VectorLimitOrderBook(const VectorLimitOrderBook&) = delete;
    VectorLimitOrderBook& operator=(const VectorLimitOrderBook&) = delete;

    VectorLimitOrderBook(VectorLimitOrderBook&&) = default;
    VectorLimitOrderBook& operator=(VectorLimitOrderBook&&) = delete;

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
    struct PriceLevel {
        int price{};
        std::vector<Order> orders{};
        std::size_t first_active_index{0};
    };

    struct OrderLocation {
        Side side{};
        int price{};
        std::size_t order_index{};
    };

    using SideLevels = std::vector<PriceLevel>;

    std::unique_ptr<Publisher<Trade>> trade_publisher;
    std::queue<Trade>& trade_events;
    std::string ticker{};

    SideLevels bids{};
    SideLevels asks{};

    mutable SideContainer bid_side_cache{};
    mutable SideContainer ask_side_cache{};

    std::unordered_map<int, OrderLocation> order_id_map{};

    void match_order(SideLevels& near_side, SideLevels& far_side, int price, int remaining_quantity,
                     int order_id, Side side, std::string_view broker_id);

    [[nodiscard]] SideLevels& get_side_mut(Side side);
    [[nodiscard]] const SideLevels& get_side_levels(Side side) const;

    std::size_t upsert_level(SideLevels& side_levels, Side side, int price);
    [[nodiscard]] std::optional<std::size_t> find_level_index(const SideLevels& side_levels, Side side,
                                                              int price) const;
    void rebuild_side_cache(const SideLevels& side_levels, SideContainer& cache) const;
    [[nodiscard]] static bool level_has_active_orders(const PriceLevel& level);
    [[nodiscard]] static std::size_t first_active_order_index(const PriceLevel& level);
};

} // namespace engine
