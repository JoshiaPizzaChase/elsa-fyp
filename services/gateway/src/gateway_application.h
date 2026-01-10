#ifndef GATEWAY_APPLICATION_H
#define GATEWAY_APPLICATION_H

#include "pods.h"
#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include <quickfix/Except.h>
#include <quickfix/fix42/OrderCancelRequest.h>

namespace gateway {

class GatewayApplication : public FIX::Application, public FIX::MessageCracker {
  public:
    GatewayApplication();

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
     * INBOUND MESSAGE HANDLERS
     * Only support FIX4.2.
     */
    void onMessage(const FIX42::NewOrderSingle&, const FIX::SessionID&) override;
    void onMessage(const FIX42::OrderCancelRequest&, const FIX::SessionID&) override;
    // TODO: Implement reject message.

    /*
     * OUTBOUND MESSAGE HANDLERS
     */
    void rejectMessage();
    void sendMessageAsync(const transport::MessagePod& pod);

  private:
};

} // namespace gateway
#endif // GATEWAY_APPLICATION_H
