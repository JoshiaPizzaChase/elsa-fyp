#include "gateway_application.h"
#include "containers.h"
#include "orders.h"
#include <optional>
#include <print>
#include <quickfix/FixFields.h>
#include <quickfix/FixValues.h>
#include <quickfix/fix42/ExecutionReport.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <stdexcept>

namespace gateway {

/* Temporarily implemented to log on invokation. */
void GatewayApplication::onCreate(const FIX::SessionID& sessionId) {
    std::println("[Gateway] Created - {}", sessionId.toString());
};

void GatewayApplication::onLogon(const FIX::SessionID& sessionId) {
    std::println("[Gateway] Logged on - {}", sessionId.toString());
};

void GatewayApplication::onLogout(const FIX::SessionID& sessionId) {
    std::println("[Gateway] Logged out - {}", sessionId.toString());
};

void GatewayApplication::toAdmin(FIX::Message& message, const FIX::SessionID& sessionId) {
    std::println("[Gateway] To admin: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::toApp(FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::DoNotSend) {
    std::println("[Gateway] To app: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::fromAdmin(const FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {
    std::println("[Gateway] From admin: {} - {}", message.toString(), sessionId.toString());
};

void GatewayApplication::fromApp(const FIX::Message& message, const FIX::SessionID& sessionId)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue,
           FIX::UnsupportedMessageType) {
    std::println("[Gateway] From app: {} - {}", message.toString(), sessionId.toString());
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

        core::NewOrderRequestContainer newOrderRequest{
            .senderCompId = senderCompId,
            .targetCompId = targetCompId,
            .clOrdId = clOrdId,
            .symbol = symbol,
            .side = core::convertToInternal(side),
            .orderQty = orderQty,
            .ordType = core::convertToInternal(ordType),
            .price = (ordType == FIX::OrdType_LIMIT) ? std::make_optional(price) : std::nullopt,
            .timeInForce = core::convertToInternal(timeInForce),
        };

        // sendContainer(newOrderRequest);

    } catch (const std::exception& e) {
        std::println(stderr, "[Gateway] Error: {}", e.what());
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
        message.get(orderId);
        message.get(origClOrdId);
        message.get(clOrdId);
        message.get(orderQty);
        message.get(symbol);
        message.get(side);

        core::CancelOrderRequestContainer cancelOrderRequest{
            .senderCompId = senderCompId,
            .targetCompId = targetCompId,
            .orderId = orderId,
            .origClOrdId = origClOrdId,
            .clOrdId = clOrdId,
            .symbol = symbol,
            .side = core::convertToInternal(side),
            .orderQty = orderQty,
        };

        // sendContainer(cancelOrderRequest);
        
    } catch (const std::exception& e) {
        std::println(stderr, "[Gateway] Error: {}", e.what());
        rejectMessage(senderCompId, targetCompId, clOrdId, symbol, side, e.what());
    }
};

void GatewayApplication::rejectMessage(const FIX::SenderCompID& sender,
                                       const FIX::TargetCompID& target, const FIX::ClOrdID& clOrdId,
                                       const FIX::Symbol& symbol, const FIX::Side& side,
                                       const std::string& rejectReason) {

    FIX::TargetCompID targetCompID(sender.getValue());
    FIX::SenderCompID senderCompID(target.getValue());

    // TODO: Set up preconditions, e.g. asserting session existence, to catch bugs in debug build.

    FIX42::ExecutionReport execReport{FIX::OrderID(clOrdId.getValue()),
                                      FIX::ExecID(m_idGenerator.genExecutionID()),
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
        std::println(stderr, "[Gateway] Error: {}", e.what());
    }
}

void GatewayApplication::sendContainer() {
    throw std::runtime_error("unimplemented");
};

} // namespace gateway
