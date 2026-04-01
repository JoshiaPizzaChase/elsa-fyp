#pragma once

namespace engine {
template <typename T>
class Publisher {
  public:
    virtual ~Publisher() = default;
    virtual bool try_publish(T& msg) = 0;
};
} // namespace engine