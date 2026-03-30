#pragma once

#include "core/orders.h"

namespace engine {

using core::Side;

class Order {
  public:
    Order(int order_id, int price, int quantity, Side side, std::string_view trader_id);

    [[nodiscard]] int get_order_id() const;
    [[nodiscard]] int get_price() const;
    [[nodiscard]] int get_quantity() const;
    [[nodiscard]] Side get_side() const;
    [[nodiscard]] const std::string& get_trader_id() const;

    void fill(int fill_quantity);

  private:
    const int order_id{};
    int price{};
    int quantity{};
    Side side{};
    std::string trader_id{};
};
} // namespace engine
