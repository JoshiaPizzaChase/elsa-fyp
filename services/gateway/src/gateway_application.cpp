#include "gateway_application.h"
#include "core/containers.h"
#include "core/orders.h"
#include <boost/uuid.hpp>
#include <optional>
#include <quickfix/FixFields.h>
#include <quickfix/FixValues.h>
#include <quickfix/fix42/ExecutionReport.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <quickfix/fix42/OrderCancelRequest.h>
#include <stdexcept>
#include <transport/messaging.h>

namespace gateway {

GatewayApplication::GatewayApplication(std::string host, int port) {
    m_websocketClient.start();
    gateway_connection_id =
        m_websocketClient.connect(std::format("ws://{}:{}", std::move(host), std::to_string(port)))
            .value();
    logger->info("Gateway started");
}

/* Temporarily implemented to log on invokation. */
void GatewayApplication::onCreate(const FIX::SessionID& sessionId) {
    logger->info("[Gateway] Created - {}", sessionId.toString());
};

void GatewayApplication::onLogon(const FIX::SessionID& sessionId) {
    logger->info("[Gateway] Logged on - {}", sessionId.toString());
};

void GatewayApplication::onLogout(const FIX::SessionID& sessionId) {
    logger->info("[Gateway] Logged out - {}", sessionId.toString());
};

void GatewayApplication::toAdmin(FIX::Message& message, const FIX::SessionID& sessionId) {
    logger->info("[Gateway] To admin: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::toApp(FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::DoNotSend) {
    logger->info("[Gateway] To app: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::fromAdmin(const FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {
    logger->info("[Gateway] From admin: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::fromApp(const FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue,
           FIX::UnsupportedMessageType) {
    crack(message, sessionId);
    logger->info("[Gateway] From app: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::onMessage(const FIX42::NewOrderSingle& message,
                                   const FIX::SessionID& sessionId) {
    FIX::SenderCompID senderCompId;
    FIX::TargetCompID targetCompId;
    FIX::ClOrdID clOrdId;
    FIX::Symbol symbol;
    FIX::Side side;
    FIX::OrderQty orderQty;
    FIX::OrdType ordType;
    FIX::Price price;
    FIX::TimeInForce timeInForce;

    // TODO: Set up preconditions, e.g. asserting session existence, to catch bugs in debug build.
    try {
        message.getHeader().get(senderCompId);
        message.getHeader().get(targetCompId);
        message.get(clOrdId);
        message.get(symbol);
        message.get(side);
        message.get(orderQty);
        message.get(ordType);
        if (ordType == FIX::OrdType_LIMIT) {
            message.get(price);
        }
        message.get(timeInForce);

        core::NewOrderSingleContainer newOrderRequest{
            .sender_comp_id = senderCompId,
            .target_comp_id = targetCompId,
            .order_id = std::nullopt,
            .cl_ord_id = std::stoi(clOrdId.getString()),
            .symbol = symbol,
            .side = core::convert_to_internal(side),
            .order_qty = core::convert_to_internal_quantity(orderQty),
            .ord_type = core::convert_to_internal(ordType),
            .price = (ordType == FIX::OrdType_LIMIT)
                         ? std::make_optional(core::convert_to_internal_price(price))
                         : std::nullopt,
            .time_in_force = core::convert_to_internal(timeInForce),
        };

        logger->info("Order received");

        sendContainer(newOrderRequest);

    } catch (const std::exception& e) {
        logger->error("[Gateway] Error: {}", e.what());

        rejectMessage(senderCompId, targetCompId, clOrdId, symbol, side, e.what());
    }
};

void GatewayApplication::onMessage(const FIX42::OrderCancelRequest& message,
                                   const FIX::SessionID& sessionId) {

    FIX::SenderCompID senderCompId;
    FIX::TargetCompID targetCompId;
    FIX::OrderID orderId;
    FIX::OrigClOrdID origClOrdId;
    FIX::ClOrdID clOrdId;
    FIX::OrderQty orderQty;
    FIX::Symbol symbol;
    FIX::Side side;

    try {
        message.getHeader().get(senderCompId);
        message.getHeader().get(targetCompId);
        if (message.isSetField(FIX::FIELD::OrderID)) {
            message.get(orderId);
        }
        message.get(origClOrdId);
        message.get(clOrdId);
        message.get(orderQty);
        message.get(symbol);
        message.get(side);

        core::CancelOrderRequestContainer cancelOrderRequest{
            .sender_comp_id = senderCompId,
            .target_comp_id = targetCompId,
            .order_id = message.isSetField(FIX::FIELD::OrderID)
                            ? std::make_optional(std::stoi(orderId.getString()))
                            : std::nullopt,
            .orig_cl_ord_id = std::stoi(origClOrdId.getString()),
            .cl_ord_id = std::stoi(clOrdId.getString()),
            .symbol = symbol,
            .side = core::convert_to_internal(side),
            .order_qty = core::convert_to_internal_quantity(orderQty),
        };

        sendContainer(cancelOrderRequest);

    } catch (const std::exception& e) {
        logger->error("[Gateway] Error: {}", e.what());

        rejectMessage(senderCompId, targetCompId, clOrdId, symbol, side, e.what());
    }
};

// TODO: Persist these rejected messages as these never go to order manager!
// TODO: Should we reject here or passthrough to order manager anyways?
void GatewayApplication::rejectMessage(const FIX::SenderCompID& sender,
                                       const FIX::TargetCompID& target, const FIX::ClOrdID& clOrdId,
                                       const FIX::Symbol& symbol, const FIX::Side& side,
                                       const std::string& rejectReason) {

    FIX::TargetCompID targetCompID(sender.getValue());
    FIX::SenderCompID senderCompID(target.getValue());

    // TODO: Set up preconditions, e.g. asserting session existence, to catch bugs in debug build.

    FIX42::ExecutionReport execReport{FIX::OrderID(clOrdId.getValue()),
                                      FIX::ExecID(to_string(boost::uuids::time_generator_v7()())),
                                      FIX::ExecTransType(FIX::ExecTransType_NEW),
                                      FIX::ExecType(FIX::ExecType_REJECTED),
                                      FIX::OrdStatus(FIX::ExecType_REJECTED),
                                      symbol,
                                      side,
                                      FIX::LeavesQty{0},
                                      FIX::CumQty{0},
                                      FIX::AvgPx{0}};

    execReport.set(clOrdId);
    execReport.set(FIX::Text(rejectReason));

    try {
        FIX::Session::sendToTarget(execReport, senderCompID, targetCompID);
    } catch (const FIX::SessionNotFound& e) {
        logger->error("[Gateway] Error: {}", e.what());
    }
}

void GatewayApplication::sendContainer(const auto& container) {
    // TODO: Handle send error
    m_websocketClient.send(gateway_connection_id, transport::serialize_container(container));
}

void GatewayApplication::process_report() {
    if (const auto new_message = m_websocketClient.dequeue_message(0); new_message.has_value()) {
        logger->info("Execution report message received");

        const auto container = transport::deserialize_container(new_message.value());

        assert(std::holds_alternative<core::ExecutionReportContainer>(container));
        const auto& r = std::get<core::ExecutionReportContainer>(container);

        // Helper lambdas for enum → string conversion
        auto side_str = [](core::Side s) -> std::string_view {
            return s == core::Side::bid ? "bid" : "ask";
        };
        auto exec_trans_type_str = [](core::ExecTransType t) -> std::string_view {
            switch (t) {
            case core::ExecTransType::exec_trans_new:
                return "new";
            case core::ExecTransType::exec_trans_cancel:
                return "cancel";
            case core::ExecTransType::exec_trans_correct:
                return "correct";
            case core::ExecTransType::exec_trans_status:
                return "status";
            }
            return "unknown";
        };
        auto exec_status_str = [](core::ExecTypeOrOrderStatus s) -> std::string_view {
            switch (s) {
            case core::ExecTypeOrOrderStatus::status_new:
                return "new";
            case core::ExecTypeOrOrderStatus::status_partially_filled:
                return "partially_filled";
            case core::ExecTypeOrOrderStatus::status_filled:
                return "filled";
            case core::ExecTypeOrOrderStatus::status_canceled:
                return "canceled";
            case core::ExecTypeOrOrderStatus::status_pending_cancel:
                return "pending_cancel";
            case core::ExecTypeOrOrderStatus::status_rejected:
                return "rejected";
            }
            return "unknown";
        };
        auto tif_str = [](core::TimeInForce t) -> std::string_view {
            return t == core::TimeInForce::day ? "day" : "gtc";
        };

        logger->info("[ExecutionReport] sender_comp_id: {}", r.sender_comp_id);
        logger->info("[ExecutionReport] target_comp_id: {}", r.target_comp_id);
        logger->info("[ExecutionReport] order_id: {}", r.order_id);
        logger->info("[ExecutionReport] cl_order_id: {}", r.cl_order_id);
        logger->info("[ExecutionReport] orig_cl_ord_id: {}", r.orig_cl_ord_id.value_or(-1));
        logger->info("[ExecutionReport] exec_id: {}", r.exec_id);
        logger->info("[ExecutionReport] exec_trans_type: {}",
                     exec_trans_type_str(r.exec_trans_type));
        logger->info("[ExecutionReport] exec_type: {}", exec_status_str(r.exec_type));
        logger->info("[ExecutionReport] ord_status: {}", exec_status_str(r.ord_status));
        logger->info("[ExecutionReport] text: {}", r.text.value_or("N/A"));
        logger->info("[ExecutionReport] symbol: {}", r.symbol);
        logger->info("[ExecutionReport] side: {}", side_str(r.side));
        logger->info("[ExecutionReport] price: {}",
                     r.price.has_value() ? std::to_string(r.price.value()) : "N/A");
        logger->info("[ExecutionReport] time_in_force: {}",
                     r.time_in_force.has_value() ? tif_str(r.time_in_force.value()) : "N/A");
        logger->info("[ExecutionReport] leaves_qty: {}", r.leaves_qty);
        logger->info("[ExecutionReport] cum_qty: {}", r.cum_qty);
        logger->info("[ExecutionReport] avg_px: {}", r.avg_px);
    }
}

} // namespace gateway
