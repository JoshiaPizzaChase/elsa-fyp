#include "core/orderbook_snapshot.h"

int main(int argc, char* argv[]) {
    OrderbookSnapshotRingBuffer buffer = OrderbookSnapshotRingBuffer::open_exist_shm(ORDERBOOK_SNAPSHOT_SHM_FILE);
    TopOrderBookLevelAggregates snapshot("APPL");
    snapshot.bid_level_aggregates[0].price = 128;
    snapshot.bid_level_aggregates[0].quantity = 1024;
    buffer.try_push(snapshot);
    return 0;
}
