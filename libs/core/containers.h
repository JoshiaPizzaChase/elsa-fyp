#pragma once

#include "orders.h"
#include <optional>
#include <string>
#include <variant>

namespace core {

// TODO: Add constructors to ensure all members won't be accidentally missed to initialized on
// creation.

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
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

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
struct CancelOrderRequestContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::optional<std::string> order_id; // For us to easily find the original order that this request intends to cancel. REQUIRED.
    std::string orig_cl_ord_id; // Original clOrdId for the order this cancel request is for. For convenience of the sender/trader. These are required but we will ignore them.
    std::string cl_ord_id; // ClOrdId for this cancel request itself. For conveninence of the sender/trader. These are required but we will ignore them.
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_8_8.html
struct ExecutionReportContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::string order_id;    // Our internal ID for the order.
    std::string cl_ord_id;
    std::string orig_cl_ord_id;
    std::string exec_id;           // Unique ID for this execution report.
    ExecTransType exec_trans_type; // Describes the type of execution report.
    ExecType exec_type;            // Order event that caused the issuance of this report.
    OrderStatus ord_status;        // Always the order status.
    std::string ord_reject_reason;
    std::string symbol;
    Side side;
    std::optional<std::int32_t> price;
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
