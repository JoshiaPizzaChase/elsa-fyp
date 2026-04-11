#pragma once

#include "limit_order_book.h"

#include <memory>
#include <vector>

namespace engine {

class FlatVectorLimitOrderBook {
  public:
    FlatVectorLimitOrderBook(std::string_view ticker, std::queue<Trade>& trade_container,
                             std::unique_ptr<Publisher<Trade>> trade_publisher);

    FlatVectorLimitOrderBook(const FlatVectorLimitOrderBook&) = delete;
    FlatVectorLimitOrderBook& operator=(const FlatVectorLimitOrderBook&) = delete;

    FlatVectorLimitOrderBook(FlatVectorLimitOrderBook&&) = default;
    FlatVectorLimitOrderBook& operator=(FlatVectorLimitOrderBook&&) = delete;

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
        std::size_t begin{};
        std::size_t end{};
        std::size_t first_active{};
    };

    struct SideStorage {
        std::vector<std::unique_ptr<Order>> orders{};
        std::vector<PriceLevel> levels{};
    };

    struct OrderLocation {
        Side side{};
        int price{};
        Order* order{};
    };

    std::unique_ptr<Publisher<Trade>> trade_publisher;
    std::queue<Trade>& trade_events;
    std::string ticker{};

    SideStorage bids{};
    SideStorage asks{};

    mutable SideContainer bid_side_cache{};
    mutable SideContainer ask_side_cache{};

    std::unordered_map<int, OrderLocation> order_id_map{};

    void match_order(SideStorage& near_side, SideStorage& far_side, int price,
                     int remaining_quantity, int order_id, Side side, std::string_view broker_id);

    [[nodiscard]] SideStorage& get_side_mut(Side side);
    [[nodiscard]] const SideStorage& get_side_storage(Side side) const;

    [[nodiscard]] std::optional<std::size_t> find_level_index(const SideStorage& side_storage, Side side,
                                                              int price) const;
    std::size_t upsert_level(SideStorage& side_storage, Side side, int price);
    void shift_levels_after_insert(std::vector<PriceLevel>& levels, std::size_t start_level_index,
                                   std::size_t insert_pos);
    [[nodiscard]] static std::size_t first_active_order_index(const SideStorage& side_storage,
                                                               const PriceLevel& level);
    [[nodiscard]] static bool has_active_orders(const SideStorage& side_storage,
                                                const PriceLevel& level);
    void rebuild_side_cache(const SideStorage& side_storage, SideContainer& cache) const;
};

} // namespace engine
