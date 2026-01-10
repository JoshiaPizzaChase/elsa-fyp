#include "gateway_application.h"
#include "containers.h"
#include "orders.h"
#include <optional>
#include <print>
#include <quickfix/FixFields.h>
#include <quickfix/FixValues.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <stdexcept>

namespace gateway {

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
    FIX::SenderCompID senderCompID;
    FIX::TargetCompID targetCompID;
    FIX::ClOrdID clOrdID;
    FIX::Account account;
    FIX::Symbol symbol;
    FIX::Side side;
    FIX::TransactTime transactTime;
    FIX::OrderQty orderQty;
    FIX::OrdType ordType;
    FIX::Price price;
    FIX::TimeInForce timeInForce;

    try {
        message.getHeader().get(senderCompID);
        message.getHeader().get(targetCompID);
        message.get(clOrdID);
        message.get(account);
        message.get(symbol);
        message.get(side);
        message.get(orderQty);
        message.get(ordType);
        message.get(price);
        message.get(timeInForce);
        if (ordType == FIX::OrdType_LIMIT) {
            message.get(price);
        }

        core::NewOrderRequestContainer newOrderRequest{
            .senderCompId = senderCompID,
            .targetCompId = targetCompID,
            .clOrdId = clOrdID,
            .account = account,
            .symbol = symbol,
            .side = core::convertToInternal(side),
            .orderQty = orderQty,
            .ordType = core::convertToInternal(ordType),
            .price = (ordType == FIX::OrdType_LIMIT) ? std::make_optional(price) : std::nullopt,
            .timeInForce = core::convertToInternal(timeInForce),
        };

        // TODO: convert newOrderRequest to binary message
        // send message async to next service

    } catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
        rejectMessage();
    }
};

void GatewayApplication::onMessage(const FIX42::OrderCancelRequest&, const FIX::SessionID&) {
    throw std::runtime_error("unimplemented");
};

void GatewayApplication::sendMessageAsync(const transport::MessagePod& pod) {
    throw std::runtime_error("unimplemented");
};

} // namespace gateway
