#pragma once

#include "message_format.h"

#include <expected>
#include <optional>
#include <string>

namespace transport {
class OutboundClient {
  public:
    virtual ~OutboundClient() = default;

    virtual std::expected<void, int> start() = 0;
    virtual std::expected<int, int> connect(std::string_view uri, std::string_view name) = 0;
    virtual std::optional<std::string> dequeue_message(int id) = 0;
    virtual std::optional<std::string> wait_and_dequeue_message(int id) = 0;
    virtual std::expected<void, int> send(int id, const std::string& payload,
                                          MessageFormat fmt = MessageFormat::binary) = 0;
};
} // namespace transport