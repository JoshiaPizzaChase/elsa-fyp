#include "order.h"
#include <boost/contract.hpp>

namespace engine {
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
    boost::contract::check c = boost::contract::function().precondition(
        [&] { BOOST_CONTRACT_ASSERT(fill_quantity <= quantity); });

    quantity -= fill_quantity;
}
} // namespace engine
