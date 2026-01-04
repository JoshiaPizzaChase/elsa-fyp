#ifndef ELSA_FYP_LIMIT_ORDER_BOOK_H
#define ELSA_FYP_LIMIT_ORDER_BOOK_H

#include <expected>
#include <map>
#include <unordered_map>
#include <list>
#include <string>

#include "order.h"

class LimitOrderBook {
public:
    std::expected<void, std::string> add_order(int order_id, int price, int quantity, Side side);
    std::expected<void, std::string> cancel_order(int order_id);
    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string> get_best_order(Side side) const;
    [[nodiscard]] std::expected<std::reference_wrapper<const Order>, std::string> get_order_by_id(int order_id) const;

private:
    std::map<int, std::list<Order>> bids;
    std::map<int, std::list<Order>> asks;

    std::unordered_map<int, std::list<Order>::const_iterator> order_id_map;

    void match_order(std::map<int, std::list<Order>>& near_side, std::map<int, std::list<Order>>& far_side, int price,
                     int quantity, int order_id, Side side);
};

#endif //ELSA_FYP_LIMIT_ORDER_BOOK_H
