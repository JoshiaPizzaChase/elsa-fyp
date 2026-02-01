#ifndef CORE_ORDERS_H
#define CORE_ORDERS_H

#include "constants.h"
#include <quickfix/FixFields.h>
#include <quickfix/FixValues.h>
#include <stdexcept>

namespace core {

enum class OrderType {
    limit,
    market
};

enum class Side {
    bid,
    ask
};

enum class TimeInForce {
    day
    // Extend here as needed.
};

// For some reason, this is also needed.
enum class ExecTransType {
    exec_trans_new,     // New information contained in execution report.
    exec_trans_cancel,  // Cancelling the execution report.
    exec_trans_correct, // Correction in information contained in previous execution report.
    exec_trans_status // No new information contained in execution report. Only a summary of status.
};

// Not supposed to be used directly.
// ExecType and OrderStatus are both required but use the same possible values.
// ExecType describes what caused the emission of report, OrderStatus always describes current
// state of orders.
enum class ExecTypeOrOrderStatus {
    status_new,
    status_partially_filled,
    status_filled,
    status_canceled,
    status_pending_cancel,
    status_rejected,
};

using ExecType = ExecTypeOrOrderStatus;
using OrderStatus = ExecTypeOrOrderStatus;

// TODO: Move these to a cpp file to prevent ODR violation and inlining everything
// Convert from FIX types to internal types
inline Side convert_to_internal(const FIX::Side& side) {
    constexpr auto error_msg{"Unsupported Side, use buy or sell"};
    assert((side == FIX::Side_BUY || side == FIX::Side_SELL) && error_msg);

    switch (side) {
    case FIX::Side_BUY:
        return Side::bid;
    case FIX::Side_SELL:
        return Side::ask;
    default:
        throw std::logic_error(error_msg);
    }
}

inline OrderType convert_to_internal(const FIX::OrdType& ord_type) {
    constexpr auto error_msg{"Unsupported Order Type, use limit or market"};
    assert((ord_type == FIX::OrdType_LIMIT || ord_type == FIX::OrdType_MARKET) && error_msg);

    switch (ord_type) {
    case FIX::OrdType_LIMIT:
        return OrderType::limit;
    case FIX::OrdType_MARKET:
        return OrderType::market;
    default:
        throw std::logic_error(error_msg);
    }
}

inline TimeInForce convert_to_internal(const FIX::TimeInForce& tif) {
    constexpr auto error_msg{"Unsupported Time In Force, use day"};
    assert(tif == FIX::TimeInForce_DAY && error_msg);

    switch (tif) {
    case FIX::TimeInForce_DAY:
        return TimeInForce::day;
    default:
        throw std::logic_error(error_msg);
    }
}

// Convert from internal types to FIX types
inline FIX::Side convert_to_fix(Side side) {
    constexpr auto error_msg{"Unsupported side"};
    assert((side == Side::bid || side == Side::ask) && error_msg);

    switch (side) {
    case Side::bid:
        return FIX::Side_BUY;
    case Side::ask:
        return FIX::Side_SELL;
    default:
        throw std::logic_error(error_msg);
    }
}

inline FIX::OrdType convert_to_fix(OrderType ord_type) {
    constexpr auto error_msg{"Unsupported order type"};
    assert((ord_type == OrderType::limit || ord_type == OrderType::market) && error_msg);

    switch (ord_type) {
    case OrderType::limit:
        return FIX::OrdType_LIMIT;
    case OrderType::market:
        return FIX::OrdType_MARKET;
    default:
        throw std::logic_error(error_msg);
    }
}

inline FIX::TimeInForce convert_to_fix(TimeInForce tif) {
    constexpr auto error_msg{"Unsupported time in force"};
    assert(tif == TimeInForce::day && error_msg);

    switch (tif) {
    case TimeInForce::day:
        return FIX::TimeInForce_DAY;
    default:
        throw std::logic_error(error_msg);
    }
}

// Input price is the true, dollar.cent amount!
inline constexpr std::int32_t convert_to_internal_price(double price) {
    return static_cast<std::int32_t>(price * constants::decimal_to_int_multiplier);
}

// Input quantity is the true, dollar.cent amount!
inline constexpr std::int32_t convert_to_internal_quantity(double quantity) {
    return static_cast<std::int32_t>(quantity * constants::decimal_to_int_multiplier);
}

} // namespace core

#endif // CORE_ORDERS_H
