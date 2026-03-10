#pragma once

#include <string>

#include "order.h"

enum class OrderStatus {
    NEW = 0,
    FILLED = 1,
    PARTIALLY_FILLED = 2,
    CANCELED = 3,
};

struct ExecutionReport {
    std::string order_id;
    std::string custom_order_id;
    std::string ticker;
    OrderSide side;
    OrderStatus status;
    double filled_qty;
    double last_px;
    double cumulated_filled_qty;
    double avg_px;
    double remaining_qty;
};
