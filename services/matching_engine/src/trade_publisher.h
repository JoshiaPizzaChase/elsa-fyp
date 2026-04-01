#pragma once
#include "core/trade.h"

namespace engine {

class TradePublisher {
  public:
    virtual ~TradePublisher() = default;
    virtual bool try_publish(Trade& trade) = 0;
};

} // namespace engine