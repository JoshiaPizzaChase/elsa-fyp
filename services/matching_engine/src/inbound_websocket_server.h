#pragma once
#include "inbound_server.h"
#include "websocket_server.h"

namespace engine {

using WebsocketManagerServer = transport::WebsocketManagerServer;

class InboundWebsocketServer : public InboundServer {
  public:
    explicit InboundWebsocketServer(std::string_view host, int port,
                                    std::shared_ptr<spdlog::logger> logger)
        : inbound_ws_server{port, host, logger} {};

    std::expected<void, int> start() override {
        return inbound_ws_server.start();
    }

    std::vector<InboundConnectionInfo> get_connection_info() const override {
        std::vector<InboundConnectionInfo> connection_info{};
        for (const auto& ws_info_map = inbound_ws_server.get_id_to_connection_map();
             const auto& [id, connection_metadata] : ws_info_map) {
            connection_info.emplace_back(id, connection_metadata->get_counter_party());
        }

        return connection_info;
    }

    std::optional<std::string> dequeue_message(int id) override {
        return inbound_ws_server.dequeue_message(id);
    }
    std::expected<void, int> send(int id, const std::string& payload,
                                  InboundMessageFormat fmt) override {
        return inbound_ws_server.send(id, payload,
                                      (fmt == InboundMessageFormat::binary)
                                          ? transport::MessageFormat::binary
                                          : transport::MessageFormat::text);
    }

  private:
    WebsocketManagerServer inbound_ws_server;
};
} // namespace engine