#pragma once

#include "outbound_client.h"
#include "websocket_client.h"

#include <spdlog/spdlog.h>

namespace transport {

using WebsocketManagerClient = WebsocketManagerClient;

class OutboundWebsocketClient : public OutboundClient {
  public:
    explicit OutboundWebsocketClient(std::shared_ptr<spdlog::logger> logger)
        : outbound_ws_client{logger} {};

    std::expected<void, int> start() override {
        return outbound_ws_client.start();
    }

    std::expected<int, int> connect(std::string_view uri, std::string_view name) override {
        return outbound_ws_client.connect(uri, name);
    }
 std::optional<std::string> dequeue_message(int id) override {
        return outbound_ws_client.dequeue_message(id);
    }

    std::optional<std::string> wait_and_dequeue_message(int id) override {
        return outbound_ws_client.wait_and_dequeue_message(id);
    }

    std::expected<void, int> send(int id, const std::string& payload, MessageFormat fmt) override {
        return outbound_ws_client.send(id, payload, fmt);
    }

  private:
    WebsocketManagerClient outbound_ws_client;
};

} // namespace transport