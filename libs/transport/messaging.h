#ifndef TRANSPORT_MESSAGING_H
#define TRANSPORT_MESSAGING_H

#include "core/containers.h"
#include "protobufs/containers.pb.h"
#include <stdexcept>

namespace transport {

// TODO: Probably good to have some kind of "abstract base class/interface" for messaging clients
// and servers (ie inter-service messaging). Later.
// Note: Such client/servers should not have to deal with specific transport protocols (eg gRPC, raw
// TCP etc). This may be a good place to use dependency injection pattern?
class OrderManagerClient {
    // TODO: complete this class. To be used by gateway.
};

class OrderManagerServer {
    // TODO: complete this class. To be used by order manager.
};

// TODO: Improve these conversion functions. Is there any way to avoid boilerplate?
transport::Side convertToProto(core::Side side) {
    switch (side) {
    case core::Side::bid:
        return transport::Side::SIDE_BID;
    case core::Side::ask:
        return transport::Side::SIDE_ASK;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

transport::OrderType convertToProto(core::OrderType ordType) {
    switch (ordType) {
    case core::OrderType::limit:
        return transport::OrderType::ORDER_TYPE_LIMIT;
    case core::OrderType::market:
        return transport::OrderType::ORDER_TYPE_MARKET;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

transport::TimeInForce convertToProto(core::TimeInForce tif) {
    switch (tif) {
    case core::TimeInForce::day:
        return transport::TimeInForce::TIF_DAY;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

transport::ExecTransType convertToProto(core::ExecTransType execTransType) {
    switch (execTransType) {
    case core::ExecTransType::exectrans_new:
        return transport::ExecTransType::EXEC_TRANS_NEW;
    case core::ExecTransType::exectrans_cancel:
        return transport::ExecTransType::EXEC_TRANS_CANCEL;
    case core::ExecTransType::exectrans_correct:
        return transport::ExecTransType::EXEC_TRANS_CORRECT;
    case core::ExecTransType::exectrans_status:
        return transport::ExecTransType::EXEC_TRANS_STATUS;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

transport::ExecTypeOrOrderStatus convertToProto(core::ExecTypeOrOrderStatus execTypeOrOrderStatus) {
    switch (execTypeOrOrderStatus) {
    case core::ExecTypeOrOrderStatus::status_new:
        return transport::ExecTypeOrOrderStatus::STATUS_NEW;
    case core::ExecTypeOrOrderStatus::status_partiallyFilled:
        return transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED;
    case core::ExecTypeOrOrderStatus::status_filled:
        return transport::ExecTypeOrOrderStatus::STATUS_FILLED;
    case core::ExecTypeOrOrderStatus::status_canceled:
        return transport::ExecTypeOrOrderStatus::STATUS_CANCELED;
    case core::ExecTypeOrOrderStatus::status_pendingCancel:
        return transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL;
    case core::ExecTypeOrOrderStatus::status_rejected:
        return transport::ExecTypeOrOrderStatus::STATUS_REJECTED;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// From proto enum to core enum
core::Side convertToInternal(transport::Side side) {
    switch (side) {
    case transport::Side::SIDE_BID:
        return core::Side::bid;
    case transport::Side::SIDE_ASK:
        return core::Side::ask;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

core::OrderType convertToInternal(transport::OrderType ordType) {
    switch (ordType) {
    case transport::OrderType::ORDER_TYPE_LIMIT:
        return core::OrderType::limit;
    case transport::OrderType::ORDER_TYPE_MARKET:
        return core::OrderType::market;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

core::TimeInForce convertToInternal(transport::TimeInForce tif) {
    switch (tif) {
    case transport::TimeInForce::TIF_DAY:
        return core::TimeInForce::day;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

core::ExecTransType convertToInternal(transport::ExecTransType execTransType) {
    switch (execTransType) {
    case transport::ExecTransType::EXEC_TRANS_NEW:
        return core::ExecTransType::exectrans_new;
    case transport::ExecTransType::EXEC_TRANS_CANCEL:
        return core::ExecTransType::exectrans_cancel;
    case transport::ExecTransType::EXEC_TRANS_CORRECT:
        return core::ExecTransType::exectrans_correct;
    case transport::ExecTransType::EXEC_TRANS_STATUS:
        return core::ExecTransType::exectrans_status;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

core::ExecTypeOrOrderStatus
convertToInternal(transport::ExecTypeOrOrderStatus execTypeOrOrderStatus) {
    switch (execTypeOrOrderStatus) {
    case transport::ExecTypeOrOrderStatus::STATUS_NEW:
        return core::ExecTypeOrOrderStatus::status_new;
    case transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED:
        return core::ExecTypeOrOrderStatus::status_partiallyFilled;
    case transport::ExecTypeOrOrderStatus::STATUS_FILLED:
        return core::ExecTypeOrOrderStatus::status_filled;
    case transport::ExecTypeOrOrderStatus::STATUS_CANCELED:
        return core::ExecTypeOrOrderStatus::status_canceled;
    case transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL:
        return core::ExecTypeOrOrderStatus::status_pendingCancel;
    case transport::ExecTypeOrOrderStatus::STATUS_REJECTED:
        return core::ExecTypeOrOrderStatus::status_rejected;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// Serializer and deserializer functions
std::string serializeContainer(const core::NewOrderSingleContainer& container) {
    transport::NewOrderSingleContainer containerProto;

    containerProto.set_cl_ord_id(container.clOrdId);
    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    containerProto.set_order_qty(container.orderQty);
    containerProto.set_ord_type(convertToProto(container.ordType));
    // Don't serialize price if not set to save some space
    if (container.price.has_value()) {
        containerProto.set_price(container.price.value());
    }
    containerProto.set_time_in_force(convertToProto(container.timeInForce));

    return containerProto.SerializeAsString();
}

std::string serializeContainer(const core::CancelOrderRequestContainer& container) {
    transport::CancelOrderRequestContainer containerProto;

    containerProto.set_cl_ord_id(container.clOrdId);
    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_order_id(container.orderId);
    containerProto.set_orig_cl_ord_id(container.origClOrdId);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    containerProto.set_order_qty(container.orderQty);

    return containerProto.SerializeAsString();
}

// TODO: Who will actually create these execution report containers? Gateway? Order Manager?
std::string serializeContainer(const core::ExecutionReportContainer& container) {
    transport::ExecutionReportContainer containerProto;

    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_order_id(container.orderId);
    containerProto.set_cl_order_id(container.clOrderId);
    if (container.origClOrdID.has_value()) {
        containerProto.set_orig_cl_ord_id(container.origClOrdID.value());
    }
    containerProto.set_exec_id(container.execId);
    // TODO: Does this kind of static_cast work between enums???
    containerProto.set_exec_trans_type(
        static_cast<transport::ExecTransType>(container.execTransType));
    containerProto.set_exec_type(static_cast<transport::ExecTypeOrOrderStatus>(container.execType));
    containerProto.set_ord_status(
        static_cast<transport::ExecTypeOrOrderStatus>(container.ordStatus));
    containerProto.set_ord_reject_reason(container.ordRejectReason);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    if (container.price.has_value()) {
        containerProto.set_price(container.price.value());
    }
    if (container.timeInForce.has_value()) {
        containerProto.set_time_in_force(
            static_cast<transport::TimeInForce>(container.timeInForce.value()));
    }
    containerProto.set_leaves_qty(container.leavesQty);
    containerProto.set_cum_qty(container.cumQty);
    containerProto.set_avg_px(container.avgPx);

    return containerProto.SerializeAsString();
}

core::NewOrderSingleContainer deserializeContainer(const std::string& data) {
    // switch on cases to find out whether data is NewOrderSingleContainer or
    // CancelOrderRequestContainer or ExecutionReportContainer
}

} // namespace transport

#endif // TRANSPORT_MESSAGING_H
