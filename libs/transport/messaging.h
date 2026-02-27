#pragma once

#include "core/containers.h"
#include "protobufs/containers.pb.h"
#include <optional>
#include <stdexcept>

namespace transport {

inline transport::Side convert_to_proto(core::Side side) {
    switch (side) {
    case core::Side::bid:
        return transport::Side::SIDE_BID;
    case core::Side::ask:
        return transport::Side::SIDE_ASK;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

inline transport::OrderType convert_to_proto(core::OrderType ord_type) {
    switch (ord_type) {
    case core::OrderType::limit:
        return transport::OrderType::ORDER_TYPE_LIMIT;
    case core::OrderType::market:
        return transport::OrderType::ORDER_TYPE_MARKET;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

inline transport::TimeInForce convert_to_proto(core::TimeInForce tif) {
    switch (tif) {
    case core::TimeInForce::day:
        return transport::TimeInForce::TIF_DAY;
    case core::TimeInForce::gtc:
        return transport::TimeInForce::TIF_GTC;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

inline transport::ExecTransType convert_to_proto(core::ExecTransType exec_trans_type) {
    switch (exec_trans_type) {
    case core::ExecTransType::exec_trans_new:
        return transport::ExecTransType::EXEC_TRANS_NEW;
    case core::ExecTransType::exec_trans_cancel:
        return transport::ExecTransType::EXEC_TRANS_CANCEL;
    case core::ExecTransType::exec_trans_correct:
        return transport::ExecTransType::EXEC_TRANS_CORRECT;
    case core::ExecTransType::exec_trans_status:
        return transport::ExecTransType::EXEC_TRANS_STATUS;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

inline transport::ExecTypeOrOrderStatus
convert_to_proto(core::ExecTypeOrOrderStatus exec_type_or_order_status) {
    switch (exec_type_or_order_status) {
    case core::ExecTypeOrOrderStatus::status_new:
        return transport::ExecTypeOrOrderStatus::STATUS_NEW;
    case core::ExecTypeOrOrderStatus::status_partially_filled:
        return transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED;
    case core::ExecTypeOrOrderStatus::status_filled:
        return transport::ExecTypeOrOrderStatus::STATUS_FILLED;
    case core::ExecTypeOrOrderStatus::status_canceled:
        return transport::ExecTypeOrOrderStatus::STATUS_CANCELED;
    case core::ExecTypeOrOrderStatus::status_pending_cancel:
        return transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL;
    case core::ExecTypeOrOrderStatus::status_rejected:
        return transport::ExecTypeOrOrderStatus::STATUS_REJECTED;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// From proto enum to core enum
inline core::Side convert_to_internal(transport::Side side) {
    switch (side) {
    case transport::Side::SIDE_BID:
        return core::Side::bid;
    case transport::Side::SIDE_ASK:
        return core::Side::ask;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

inline core::OrderType convert_to_internal(transport::OrderType ord_type) {
    switch (ord_type) {
    case transport::OrderType::ORDER_TYPE_LIMIT:
        return core::OrderType::limit;
    case transport::OrderType::ORDER_TYPE_MARKET:
        return core::OrderType::market;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

inline core::TimeInForce convert_to_internal(transport::TimeInForce tif) {
    switch (tif) {
    case transport::TimeInForce::TIF_DAY:
        return core::TimeInForce::day;
    case transport::TimeInForce::TIF_GTC:
        return core::TimeInForce::gtc;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

inline core::ExecTransType convert_to_internal(transport::ExecTransType exec_trans_type) {
    switch (exec_trans_type) {
    case transport::ExecTransType::EXEC_TRANS_NEW:
        return core::ExecTransType::exec_trans_new;
    case transport::ExecTransType::EXEC_TRANS_CANCEL:
        return core::ExecTransType::exec_trans_cancel;
    case transport::ExecTransType::EXEC_TRANS_CORRECT:
        return core::ExecTransType::exec_trans_correct;
    case transport::ExecTransType::EXEC_TRANS_STATUS:
        return core::ExecTransType::exec_trans_status;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

inline core::ExecTypeOrOrderStatus
convert_to_internal(transport::ExecTypeOrOrderStatus exec_type_or_order_status) {
    switch (exec_type_or_order_status) {
    case transport::ExecTypeOrOrderStatus::STATUS_NEW:
        return core::ExecTypeOrOrderStatus::status_new;
    case transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED:
        return core::ExecTypeOrOrderStatus::status_partially_filled;
    case transport::ExecTypeOrOrderStatus::STATUS_FILLED:
        return core::ExecTypeOrOrderStatus::status_filled;
    case transport::ExecTypeOrOrderStatus::STATUS_CANCELED:
        return core::ExecTypeOrOrderStatus::status_canceled;
    case transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL:
        return core::ExecTypeOrOrderStatus::status_pending_cancel;
    case transport::ExecTypeOrOrderStatus::STATUS_REJECTED:
        return core::ExecTypeOrOrderStatus::status_rejected;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// Serializer and deserializer functions
inline std::string serialize_container(const core::FillCostQueryContainer& container) {
    transport::ContainerWrapper container_wrapper;
    transport::FillCostQueryContainer container_proto;

    container_proto.set_quantity(container.quantity);
    container_proto.set_side(convert_to_proto(container.side));
    container_proto.set_symbol(container.symbol);

    *container_wrapper.mutable_fill_cost_query() = container_proto;
    return container_wrapper.SerializeAsString();
}

inline std::string serialize_container(const core::FillCostResponseContainer& container) {
    transport::ContainerWrapper container_wrapper;
    transport::FillCostResponseContainer container_proto;

    if (container.total_cost.has_value()) {
        container_proto.set_total_cost(container.total_cost.value());
    }

    *container_wrapper.mutable_fill_cost_response() = container_proto;
    return container_wrapper.SerializeAsString();
}

inline std::string serialize_container(const core::NewOrderSingleContainer& container) {
    transport::ContainerWrapper container_wrapper;
    transport::NewOrderSingleContainer container_proto;

    container_proto.set_cl_ord_id(container.cl_ord_id);
    container_proto.set_sender_comp_id(container.sender_comp_id);
    container_proto.set_target_comp_id(container.target_comp_id);
    container_proto.set_symbol(container.symbol);
    container_proto.set_side(convert_to_proto(container.side));
    container_proto.set_order_qty(container.order_qty);
    container_proto.set_ord_type(convert_to_proto(container.ord_type));
    // Don't serialize price if not set to save some space
    if (container.price.has_value()) {
        container_proto.set_price(container.price.value());
    }
    container_proto.set_time_in_force(convert_to_proto(container.time_in_force));
    *container_wrapper.mutable_new_order_single() = container_proto;

    return container_wrapper.SerializeAsString();
}

inline std::string serialize_container(const core::CancelOrderRequestContainer& container) {
    transport::CancelOrderRequestContainer container_proto;

    container_proto.set_cl_ord_id(container.cl_ord_id);
    container_proto.set_sender_comp_id(container.sender_comp_id);
    container_proto.set_target_comp_id(container.target_comp_id);
    container_proto.set_order_id(container.order_id);
    container_proto.set_orig_cl_ord_id(container.orig_cl_ord_id);
    container_proto.set_symbol(container.symbol);
    container_proto.set_side(convert_to_proto(container.side));
    container_proto.set_order_qty(container.order_qty);

    return container_proto.SerializeAsString();
}

inline std::string serialize_container(const core::ExecutionReportContainer& container) {
    transport::ExecutionReportContainer container_proto;

    container_proto.set_sender_comp_id(container.sender_comp_id);
    container_proto.set_target_comp_id(container.target_comp_id);
    container_proto.set_order_id(container.order_id);
    container_proto.set_cl_order_id(container.cl_order_id);
    if (container.orig_cl_ord_id.has_value()) {
        container_proto.set_orig_cl_ord_id(container.orig_cl_ord_id.value());
    }
    container_proto.set_exec_id(container.exec_id);
    container_proto.set_exec_trans_type(convert_to_proto(container.exec_trans_type));
    container_proto.set_exec_type(convert_to_proto(container.exec_type));
    container_proto.set_ord_status(convert_to_proto(container.ord_status));
    container_proto.set_ord_reject_reason(container.ord_reject_reason);
    container_proto.set_symbol(container.symbol);
    container_proto.set_side(convert_to_proto(container.side));
    if (container.price.has_value()) {
        container_proto.set_price(container.price.value());
    }
    if (container.time_in_force.has_value()) {
        container_proto.set_time_in_force(convert_to_proto(container.time_in_force.value()));
    }
    container_proto.set_leaves_qty(container.leaves_qty);
    container_proto.set_cum_qty(container.cum_qty);
    container_proto.set_avg_px(container.avg_px);

    return container_proto.SerializeAsString();
}

inline core::Container deserialize_container(const std::string& data) {
    transport::ContainerWrapper container_wrapper;

    if (!container_wrapper.ParseFromString(data)) {
        throw std::invalid_argument("Failed to parse ContainerWrapper from string");
    }

    switch (container_wrapper.container_case()) {
    case transport::ContainerWrapper::kNewOrderSingle: {
        const auto& proto = container_wrapper.new_order_single();
        core::NewOrderSingleContainer container;
        container.cl_ord_id = proto.cl_ord_id();
        container.sender_comp_id = proto.sender_comp_id();
        container.target_comp_id = proto.target_comp_id();
        container.symbol = proto.symbol();
        container.side = convert_to_internal(proto.side());
        container.order_qty = proto.order_qty();
        container.ord_type = convert_to_internal(proto.ord_type());
        if (proto.has_price()) {
            container.price = proto.price();
        }
        container.time_in_force = convert_to_internal(proto.time_in_force());
        return container;
    }
    case transport::ContainerWrapper::kCancelOrderRequest: {
        const auto& proto = container_wrapper.cancel_order_request();
        core::CancelOrderRequestContainer container;
        container.cl_ord_id = proto.cl_ord_id();
        container.sender_comp_id = proto.sender_comp_id();
        container.target_comp_id = proto.target_comp_id();
        container.order_id = proto.order_id();
        container.orig_cl_ord_id = proto.orig_cl_ord_id();
        container.symbol = proto.symbol();
        container.side = convert_to_internal(proto.side());
        container.order_qty = proto.order_qty();
        return container;
    }
    case transport::ContainerWrapper::kExecutionReport: {
        const auto& proto = container_wrapper.execution_report();
        core::ExecutionReportContainer container;
        container.sender_comp_id = proto.sender_comp_id();
        container.target_comp_id = proto.target_comp_id();
        container.order_id = proto.order_id();
        container.cl_order_id = proto.cl_order_id();
        if (proto.has_orig_cl_ord_id()) {
            container.orig_cl_ord_id = proto.orig_cl_ord_id();
        }
        container.exec_id = proto.exec_id();
        container.exec_trans_type = convert_to_internal(proto.exec_trans_type());
        container.exec_type = convert_to_internal(proto.exec_type());
        container.ord_status = convert_to_internal(proto.ord_status());
        container.ord_reject_reason = proto.ord_reject_reason();
        container.symbol = proto.symbol();
        container.side = convert_to_internal(proto.side());
        if (proto.has_price()) {
            container.price = proto.price();
        }
        if (proto.has_time_in_force()) {
            container.time_in_force = convert_to_internal(proto.time_in_force());
        }
        container.leaves_qty = proto.leaves_qty();
        container.cum_qty = proto.cum_qty();
        container.avg_px = proto.avg_px();

        return container;
    }
    case transport::ContainerWrapper::kFillCostQuery: {
        const auto& proto = container_wrapper.fill_cost_query();
        core::FillCostQueryContainer container;
        container.symbol = proto.symbol();
        container.side = convert_to_internal(proto.side());
        container.quantity = proto.quantity();
        return container;
    }
    case transport::ContainerWrapper::kFillCostResponse: {
        const auto& proto = container_wrapper.fill_cost_response();
        core::FillCostResponseContainer container;
        if (proto.has_total_cost()) {
            container.total_cost = proto.total_cost();
        }
        return container;
    }

    default:
        throw std::invalid_argument("Unknown ContainerWrapper case");
    }
}

} // namespace transport
