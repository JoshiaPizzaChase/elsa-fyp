#ifndef CORE_CONTAINERS_H
#define CORE_CONTAINERS_H

#include "orders.h"
#include <optional>
#include <stdfloat>
#include <string>

namespace core {

// TODO: Add constructors to ensure all members won't be accidentally missed to initialized on
// creation.
// TODO: Extract an abstract base struct with sender and target comp ids to enable dynamic dispatch.
// TODO: Consider trimming these containers. We probably don't need it to be a one-to-one mapping with FIX protocol, and depends on what we need downstream.

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
struct NewOrderRequestContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string clOrdId;
    std::string symbol;
    Side side;
    double orderQty;
    OrderType ordType;
    std::optional<double> price; // Only required for limit orders. No price for market orders.
    TimeInForce timeInForce;
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
struct CancelOrderRequestContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string orderId;
    std::string origClOrdId; // Original clOrdId for the order this cancel request is for.
    std::string clOrdId;
    std::string symbol;
    Side side;
    double orderQty;
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_8_8.html
struct ExecutionReportContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string orderId;                    // Our internal ID for the order.
    std::string clOrderId;                  // Client-defined.
    std::optional<std::string> origClOrdID; // Only required for response to cancel order requests.
    std::string execId;                     // Unique ID for this execution report.
    ExecTransType execTransType;            // Describes the type of execution report.
    ExecType execType;                      // Order event that caused the issuance of this report.
    OrderStatus ordStatus;                  // Always the order status.
    std::string ordRejectReason;
    std::string symbol;
    Side side;
    std::optional<double> price; // Only required in response to limit orders.
    std::optional<TimeInForce> timeInForce;
    double leavesQty;
    double cumQty;
    double avgPx;
};

} // namespace core

#endif // CORE_CONTAINERS_H
