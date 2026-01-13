#ifndef GATEWAY_APPLICATION_H
#define GATEWAY_APPLICATION_H

#include "core/containers.h"
#include "id_generator.h"
#include "transport/messaging.h"
#include <quickfix/Application.h>
#include <quickfix/Except.h>
#include <quickfix/FixCommonFields.h>
#include <quickfix/FixFields.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/fix42/OrderCancelRequest.h>

namespace gateway {

class GatewayApplication : public FIX::Application, public FIX::MessageCracker {
  public:
    // TODO: Initialise websocket client properly
    GatewayApplication() = default;

    void onCreate(const FIX::SessionID&) override;
    void onLogon(const FIX::SessionID&) override;
    void onLogout(const FIX::SessionID&) override;
    void toAdmin(FIX::Message&, const FIX::SessionID&) override;
    void toApp(FIX::Message&, const FIX::SessionID&) override EXCEPT(FIX::DoNotSend);
    void fromAdmin(const FIX::Message&, const FIX::SessionID&) override
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue,
               FIX::RejectLogon);
    void fromApp(const FIX::Message&, const FIX::SessionID&) override
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue,
               FIX::UnsupportedMessageType);

    /*
     * INBOUND MESSAGE HANDLERS.
     * To handle new FIX messages, e.g. cancel-replace requests, add a new message handler here.
     */
    void onMessage(const FIX42::NewOrderSingle&, const FIX::SessionID&) override;
    void onMessage(const FIX42::OrderCancelRequest&, const FIX::SessionID&) override;

    /*
     * OUTBOUND MESSAGE HANDLERS
     */
    void rejectMessage(const FIX::SenderCompID& sender, const FIX::TargetCompID& target,
                       const FIX::ClOrdID& clOrdId, const FIX::Symbol& symbol,
                       const FIX::Side& side, const std::string& rejectReason);
    // TODO: Refactor to nicer design pattern later.
    void sendContainer(const core::NewOrderSingleContainer& container);
    void sendContainer(const core::CancelOrderRequestContainer& container);
    void sendContainer(const core::ExecutionReportContainer& container);

  private:
    IDGenerator m_idGenerator;
    transport::OrderManagerClient orderManagerClient;
};

} // namespace gateway
#endif // GATEWAY_APPLICATION_H
