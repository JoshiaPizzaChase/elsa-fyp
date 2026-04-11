#include "order.h"
#include <boost/contract.hpp>

namespace engine {
Order::Order(int order_id, int price, int quantity, Side side, std::string_view trader_id)
    : order_id{order_id}, price{price}, quantity{quantity}, side{side}, trader_id{trader_id} {
}

Order::Order(std::allocator_arg_t, const allocator_type& alloc, int order_id, int price, int quantity,
             Side side, std::string_view trader_id)
    : order_id{order_id},
      price{price},
      quantity{quantity},
      side{side},
      trader_id{trader_id, alloc.resource()} {
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

const std::pmr::string& Order::get_trader_id() const {
    return trader_id;
}

void Order::fill(int fill_quantity) {
    boost::contract::check c = boost::contract::function().precondition(
        [&] { BOOST_CONTRACT_ASSERT(fill_quantity <= quantity); });

    quantity -= fill_quantity;
}
} // namespace engine
