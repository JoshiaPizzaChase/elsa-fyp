#pragma once
#include "publisher.h"

#include <algorithm>

namespace engine {
template <typename T, typename RingBufferT>
class SharedMemoryPublisher final : public Publisher<T> {
  public:
    explicit SharedMemoryPublisher(RingBufferT ring_buffer) : ring_buffer(std::move(ring_buffer)) {
    }
    bool try_publish(T& msg) override {
        return ring_buffer.try_push(msg);
    }

  private:
    RingBufferT ring_buffer;
};
} // namespace engine