#pragma once

#include "orders.h"
#include <optional>
#include <string>
#include <variant>

namespace core {

struct NewOrderSingleContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::optional<int> order_id;
    int cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
    OrderType ord_type;
    std::optional<std::int32_t>
        price; // Only required for limit orders. No price for market orders.
    TimeInForce time_in_force;
};

struct CancelOrderRequestContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::optional<int> order_id;
    int orig_cl_ord_id; // Original clOrdId for the order this cancel request is for.
    int cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
};

struct ExecutionReportContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    int order_id;                      // Our std::int32_ternal ID for the order.
    int cl_order_id;                   // Client-defined.
    std::optional<int> orig_cl_ord_id; // Only required for response to cancel order requests.
    std::string exec_id;               // Unique ID for this execution report.
    ExecTransType exec_trans_type;     // Describes the type of execution report.
    ExecType exec_type;                // Order event that caused the issuance of this report.
    OrderStatus ord_status;            // Always the order status.
    std::optional<std::string> ord_reject_reason;
    std::string symbol;
    Side side;
    std::optional<std::int32_t> price; // Only required in response to limit orders.
    std::optional<TimeInForce> time_in_force;
    std::int32_t leaves_qty;
    std::int32_t cum_qty;
    std::int32_t avg_px;
};

struct FillCostQueryContainer {
    std::string symbol;
    std::int32_t quantity;
    Side side;
};

struct FillCostResponseContainer {
    // Optional because Matching Engine may fail to retreive data.
    std::optional<std::int32_t> total_cost;
};

struct TradeContainer {
    std::string ticker{};
    int price{0};
    int quantity{0};
    std::string trade_id{};
    std::string taker_id{};
    std::string maker_id{};
    int taker_order_id{0};
    int maker_order_id{0};
    bool is_taker_buyer{false};
};

struct CancelOrderResponseContainer {
    int order_id;
    int cl_ord_id;
    bool success;
};

using Container = std::variant<core::NewOrderSingleContainer, core::CancelOrderRequestContainer,
                               core::ExecutionReportContainer, core::FillCostQueryContainer,
                               core::FillCostResponseContainer, core::TradeContainer,
                               core::CancelOrderResponseContainer>;

} // namespace core
