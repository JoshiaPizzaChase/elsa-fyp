#include "core/orderbook_snapshot.h"
#include <iostream>

int main() {
    auto ring_buffer =
        OrderbookSnapshotRingBuffer::open_exist_shm(core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE);
    auto snapshot = TopOrderBookLevelAggregates{"APPL"};

    for (int i = 0; i < 1024; i++) {
        int j = i % 50;
        snapshot.bid_level_aggregates[j].price = i;
        snapshot.bid_level_aggregates[j].quantity = i;
        snapshot.ask_level_aggregates[j].price = i + 1;
        snapshot.ask_level_aggregates[j].quantity = i + 1;
        if (!ring_buffer.try_push(snapshot)) {
            std::cerr << "Failed to push snapshot " << "\n";
        }
    }
}
