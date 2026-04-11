#pragma once

#include "core/orders.h"
#include <memory_resource>

namespace engine {

using core::Side;

class Order {
  public:
    using allocator_type = std::pmr::polymorphic_allocator<Order>;

    Order(int order_id, int price, int quantity, Side side, std::string_view trader_id);
    Order(std::allocator_arg_t, const allocator_type& alloc, int order_id, int price, int quantity,
          Side side, std::string_view trader_id);

    [[nodiscard]] int get_order_id() const;
    [[nodiscard]] int get_price() const;
    [[nodiscard]] int get_quantity() const;
    [[nodiscard]] Side get_side() const;
    [[nodiscard]] const std::pmr::string& get_trader_id() const;

    void fill(int fill_quantity);

  private:
    const int order_id{};
    int price{};
    int quantity{};
    Side side{};
    std::pmr::string trader_id{};
};
} // namespace engine
