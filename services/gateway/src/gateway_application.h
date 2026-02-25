#pragma once

#include "core/containers.h"
#include "id_generator.h"
#include "websocket_client.h"
#include <quickfix/Application.h>
#include <quickfix/Except.h>
#include <quickfix/FixCommonFields.h>
#include <quickfix/FixFields.h>
#include <quickfix/MessageCracker.h>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace gateway {

class GatewayApplication : public FIX::Application, public FIX::MessageCracker {
  public:
    // TODO: Initialise websocket client properly
    GatewayApplication(std::string host, int port);

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

  private:
    std::shared_ptr<spdlog::logger> logger = spdlog::basic_logger_mt<spdlog::async_factory>(
        "gateway_logger", std::string{PROJECT_SOURCE_DIR} + "/logs/gateway.log");
    IDGenerator m_idGenerator;
    transport::WebsocketManagerClient m_websocketClient{logger};
    int gateway_connection_id{};

    void sendContainer(const auto& container);
};

} // namespace gateway
