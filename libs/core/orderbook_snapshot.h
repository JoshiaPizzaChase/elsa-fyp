#ifndef ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H
#define ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H

#include "inter_process/mpsc_shared_memory_ring_buffer.h"
#include <array>
#include <iostream>

constexpr int ORDER_BOOK_AGGREGATE_LEVELS = 50;

struct LevelAggregate {
    int price{0};
    int quantity{0};
};

struct TopOrderBookLevelAggregates {
    std::array<LevelAggregate, ORDER_BOOK_AGGREGATE_LEVELS> bid_level_aggregates;
    std::array<LevelAggregate, ORDER_BOOK_AGGREGATE_LEVELS> ask_level_aggregates;

    friend std::ostream& operator<<(std::ostream& os,
                                    const TopOrderBookLevelAggregates& aggregates) {
        os << "bids:[";
        for (const auto& level : aggregates.bid_level_aggregates) {
            os << level.quantity << "@" << level.price << ",";
        }
        os << "],asks:[";
        for (const auto& level : aggregates.ask_level_aggregates) {
            os << level.quantity << "@" << level.price << ",";
        }
        os << "]";
        return os;
    }
};

// typed ring buffer for communication with MDP
static std::string ORDERBOOK_SNAPSHOT_SHM_FILE = "orderbook_snapshot";
using OrderbookSnapshotRingBuffer = MpscSharedMemoryRingBuffer<TopOrderBookLevelAggregates, 1024>;

#endif // ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H
