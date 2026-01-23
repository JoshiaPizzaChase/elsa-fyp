#ifndef ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
#define ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H

#include "core/orderbook_snapshot.h"
#include "transport/websocket_server.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class MarketDataProcessor {
private:
    OrderbookSnapshotRingBuffer orderbook_snapshot_ring_buffer;

}

#endif // ELSA_FYP_CLIENT_SDK_MARKET_DATA_PROCESSOR_H
