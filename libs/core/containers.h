#pragma once

#include "orders.h"
#include <optional>
#include <string>
#include <variant>

namespace core {

struct NewOrderSingleContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::string cl_ord_id;
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
    std::string orig_cl_ord_id; // Original clOrdId for the order this cancel request is for.
    std::string cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
};

struct ExecutionReportContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::string order_id;    // Our std::int32_ternal ID for the order.
    std::string cl_order_id; // Client-defined.
    std::optional<std::string>
        orig_cl_ord_id;            // Only required for response to cancel order requests.
    std::string exec_id;           // Unique ID for this execution report.
    ExecTransType exec_trans_type; // Describes the type of execution report.
    ExecType exec_type;            // Order event that caused the issuance of this report.
    OrderStatus ord_status;        // Always the order status.
    std::string ord_reject_reason;
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

using Container = std::variant<core::NewOrderSingleContainer, core::CancelOrderRequestContainer,
                               core::ExecutionReportContainer, core::FillCostQueryContainer,
                               core::FillCostResponseContainer>;

} // namespace core
