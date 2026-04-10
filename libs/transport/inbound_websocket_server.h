#pragma once
#include "inbound_server.h"
#include "websocket_server.h"

#include <utility>

namespace transport {

using WebsocketManagerServer = WebsocketManagerServer;

class InboundWebsocketServer : public InboundServer {
  public:
    explicit InboundWebsocketServer(
        std::string_view host, int port, std::shared_ptr<spdlog::logger> logger,
        bool reuse_addr = true,
        std::optional<
            std::function<void(WebsocketManagerServer::ConnectionMetadata::conn_meta_shared_ptr)>>
            on_connection_callback = std::nullopt)
        : inbound_ws_server{port, host, std::move(logger), reuse_addr,
                            std::move(on_connection_callback)} {};

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
    std::expected<void, int> send(int id, const std::string& payload, MessageFormat fmt) override {
        return inbound_ws_server.send(id, payload, fmt);
    }

  private:
    WebsocketManagerServer inbound_ws_server;
};
} // namespace transport