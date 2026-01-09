#ifndef CORE_ORDERS_H
#define CORE_ORDERS_H

namespace core {
    enum class OrderType {
        limit,
        market
    };

    enum class Side {
        bid,
        ask
    };
}

#endif // !CORE_ORDERS_H
