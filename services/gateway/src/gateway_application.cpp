#include "gateway_application.h"

namespace gateway {

void GatewayApplication::onCreate(const FIX::SessionID&) {};
void GatewayApplication::onLogon(const FIX::SessionID& sessionID) {};
void GatewayApplication::onLogout(const FIX::SessionID& sessionID) {};
void GatewayApplication::toAdmin(FIX::Message&, const FIX::SessionID&) {};
void GatewayApplication::toApp(FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::DoNotSend) {};
void GatewayApplication::fromAdmin(const FIX::Message&, const FIX::SessionID&)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {
    };
void GatewayApplication::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID)
    EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue,
           FIX::UnsupportedMessageType) {};

void GatewayApplication::onMessage(const FIX42::NewOrderSingle&, const FIX::SessionID&) {};
void GatewayApplication::onMessage(const FIX42::OrderCancelRequest&, const FIX::SessionID&) {};


} // namespace gateway
