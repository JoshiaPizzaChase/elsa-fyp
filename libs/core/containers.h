#ifndef CORE_CONTAINERS_H
#define CORE_CONTAINERS_H

#include "orders.h"
#include <cstdint>
#include <optional>
#include <stdfloat>
#include <string>

namespace core {

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
struct NewOrderRequestContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string clOrdId;
    std::string account;
    std::string symbol;
    Side side;
    double orderQty;
    OrderType ordType;
    std::optional<double> price;
    TimeInForce timeInForce;
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_f_70.html
struct CancelOrderRequestContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string orgClOrdId;
    std::string clOrdId;
    std::string account;
    std::int64_t orderQty; // orderQty = cumQty (how many filled already) + leavesQty (how much to
                           // leave for execution)
};

// https://www.onixs.biz/fix-dictionary/4.2/msgtype_8_8.html
struct ExecutionReportContainer {
    std::string senderCompId;
    std::string targetCompId;
    std::string orderId;
    std::string clOrderId;
    std::optional<std::string> origClOrdID;
    std::string execId;
    ExecTransType execTransType;
    ExecType execType;
    OrderStatus ordStatus;
    std::string ordRejectReason;
    std::string account;
    std::string symbol;
    Side side;
    std::optional<double> price;
    std::optional<TimeInForce> timeInForce;
    double leavesQty;
    double cumQty;
    double avgPx;
};

} // namespace core

#endif // CORE_CONTAINERS_H
