#pragma once

#include "orders.h"
#include <optional>
#include <string>
#include <variant>

namespace core {

// TODO: Add constructors to ensure all members won't be accidentally missed to initialized on
// creation.
// TODO: Idea: Extract an abstract base struct with sender and target comp ids to enable dynamic
// dispatch?
// TODO: Idea 2: Consider using designated initializers (C++20) for better readability on
// construction?
// TODO: Idea 3: Consider using std::variant?
// TODO: Consider trimming these containers. We probably don't need it to be a one-to-one mapping
// with FIX protocol, and depends on what we need downstream.

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
    std::string order_id;
    std::string orig_cl_ord_id; // Original clOrdId for the order this cancel request is for.
    std::string cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_8_8.html
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

using Container = std::variant<core::NewOrderSingleContainer, core::CancelOrderRequestContainer,
                               core::ExecutionReportContainer>;

} // namespace core
