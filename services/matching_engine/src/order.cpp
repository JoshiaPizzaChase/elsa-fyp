#include "order.h"

#include <stdexcept>

Order::Order(int order_id, int price, int quantity, Side side)
    : order_id{order_id}, price{price}, quantity{quantity}, side{side} {
}

int Order::get_order_id() const {
    return order_id;
}

int Order::get_price() const {
    return price;
}

int Order::get_quantity() const {
    return quantity;
}

Side Order::get_side() const {
    return side;
}

void Order::fill(int fill_quantity) {
    if (fill_quantity > quantity) {
        throw std::invalid_argument("fill_quantity > quantity");
    }

    quantity -= fill_quantity;
}
