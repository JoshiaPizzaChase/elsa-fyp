#pragma once

#include "core/trade.h"
#include "trade_publisher.h"

namespace engine {

class ShmTradePublisher : public TradePublisher {
  public:
    explicit ShmTradePublisher(TradeRingBuffer ring_buffer) : ring_buffer{std::move(ring_buffer)} {
    }

    bool try_publish(Trade& trade) override {
        return ring_buffer.try_push(trade);
    }

  private:
    TradeRingBuffer ring_buffer;
};
} // namespace engine