#pragma once
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace engine {

enum class InboundMessageFormat {
    text,
    binary
};

struct InboundConnectionInfo {
    int id;
    std::string counter_party;
};

class InboundServer {
  public:
    virtual ~InboundServer() = default;

    virtual std::expected<void, int> start() = 0;
    virtual std::vector<InboundConnectionInfo> get_connection_info() const = 0;
    virtual std::optional<std::string> dequeue_message(int id) = 0;
    virtual std::expected<void, int>
    send(int id, const std::string& payload,
         InboundMessageFormat fmt = InboundMessageFormat::binary) = 0;
};
} // namespace engine