#ifndef ELSA_FYP_ORDER_H
#define ELSA_FYP_ORDER_H

namespace engine {
enum class Side {
    Bid,
    Ask
};

class Order {
  public:
    Order(int order_id, int price, int quantity, Side side);

    [[nodiscard]] int get_order_id() const;
    [[nodiscard]] int get_price() const;
    [[nodiscard]] int get_quantity() const;
    [[nodiscard]] Side get_side() const;

    void fill(int fill_quantity);

  private:
    const int order_id{};
    int price{};
    int quantity{};
    Side side{};
};
} // namespace engine

#endif // ELSA_FYP_ORDER_H
