#include <iostream>
#include "core/orderbook_snapshot.h"

int main(int argc, char* argv[]) {
    auto ring_buffer = OrderbookSnapshotRingBuffer::create(ORDERBOOK_SNAPSHOT_SHM_FILE);
    while (true) {
        auto result = ring_buffer->try_pop();
        if (result.has_value()) {
            std::cout << result.value() << std::endl;
        }
    }

    return 0;
}
