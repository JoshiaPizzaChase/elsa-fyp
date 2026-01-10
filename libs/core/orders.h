#ifndef CORE_ORDERS_H
#define CORE_ORDERS_H

#include <chrono>
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
    neww,
    cancel,
};

// Not supposed to be used directly.
// For some reason, ExecType and OrderStatus are both required but use the same possible values.
enum class ExecTypeOrOrderStatus {
    neww,
    partiallyFilled,
    filled,
    canceled,
    pendingCancel,
    rejected,
};

using ExecType = ExecTypeOrOrderStatus;
using OrderStatus = ExecTypeOrOrderStatus;

// TODO: Move these to a cpp file to prevent ODR violation

// Convert from FIX types to internal types
Side convertToInternal(const FIX::Side& side) {
    constexpr auto errorMsg{"Unsupported Side, use buy or sell"};
    assert((side == FIX::Side_BUY || side == FIX::Side_SELL) && errorMsg);

    switch (side) {
    case FIX::Side_BUY:
        return Side::bid;
    case FIX::Side_SELL:
        return Side::ask;
    default:
        throw std::logic_error(errorMsg);
    }
}

OrderType convertToInternal(const FIX::OrdType& ordType) {
    constexpr auto errorMsg{"Unsupported Order Type, use limit or market"};
    assert((ordType == FIX::OrdType_LIMIT || ordType == FIX::OrdType_MARKET) && errorMsg);

    switch (ordType) {
    case FIX::OrdType_LIMIT:
        return OrderType::limit;
    case FIX::OrdType_MARKET:
        return OrderType::market;
    default:
        throw std::logic_error(errorMsg);
    }
}

TimeInForce convertToInternal(const FIX::TimeInForce& tif) {
    constexpr auto errorMsg{"Unsupported Time In Force, use day"};
    assert(tif == FIX::TimeInForce_DAY && errorMsg);

    switch (tif) {
    case FIX::TimeInForce_DAY:
        return TimeInForce::day;
    default:
        throw std::logic_error(errorMsg);
    }
}

// Convert from internal types to FIX types
FIX::Side convertToFIX(Side side) {
    constexpr auto errorMsg{"Unsupported side"};
    assert((side == Side::bid || side == Side::ask) && errorMsg);

    switch (side) {
    case Side::bid:
        return FIX::Side_BUY;
    case Side::ask:
        return FIX::Side_SELL;
    default:
        throw std::logic_error(errorMsg);
    }
}

FIX::OrdType convertToFIX(OrderType ordType) {
    constexpr auto errorMsg{"Unsupported order type"};
    assert((ordType == OrderType::limit || ordType == OrderType::market) && errorMsg);

    switch (ordType) {
    case OrderType::limit:
        return FIX::OrdType_LIMIT;
    case OrderType::market:
        return FIX::OrdType_MARKET;
    default:
        throw std::logic_error(errorMsg);
    }
}

FIX::TimeInForce convertToFIX(TimeInForce tif) {
    constexpr auto errorMsg{"Unsupported time in force"};
    assert(tif == TimeInForce::day && errorMsg);

    switch (tif) {
    case TimeInForce::day:
        return FIX::TimeInForce_DAY;
    default:
        throw std::logic_error(errorMsg);
    }
}

} // namespace core

#endif // CORE_ORDERS_H
