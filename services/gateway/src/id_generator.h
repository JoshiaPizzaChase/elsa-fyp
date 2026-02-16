#ifndef GATEWAY_ID_GENERATOR_H
#define GATEWAY_ID_GENERATOR_H

#include <string>

namespace gateway {
class IDGenerator {
  public:
    IDGenerator() = default;

    std::string genExecutionId() {
        // TODO: Replace with uuid + gateway id
        return std::to_string(++m_executionId);
    }

  private:
    long m_executionId{};
};
} // namespace gateway
#endif // GATEWAY_ID_GENERATOR_H
