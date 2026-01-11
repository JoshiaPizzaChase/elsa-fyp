#ifndef GATEWAY_ID_GENERATOR_H
#define GATEWAY_ID_GENERATOR_H

#include <chrono>
#include <string>

namespace gateway {
class IDGenerator {
  public:
    IDGenerator() = default;

    std::string genExecutionID() {
        const auto now_utc = std::format("{:%Y%m%d%H%M}", std::chrono::utc_clock::now());
        // adding gateway and date to maintain uniqueness across days and services
        return std::format("gateway_{}_{}", now_utc, std::to_string(++m_executionID));
    }

  private:
    long m_executionID{};
};
} // namespace gateway
#endif // GATEWAY_ID_GENERATOR_H
