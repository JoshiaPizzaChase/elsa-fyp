#ifndef ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H
#define ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H

#include "constants.h"
#include "inter_process/mpsc_shared_memory_ring_buffer.h"
#include "nlohmann/json.hpp"
#include "orders.h"
#include <array>
#include <cassert>

using json = nlohmann::json;

struct LevelAggregate {
    int price{0}; // NOTE: this price is real price multiply by decimal_to_int_multiplier
    int quantity{0};
};

struct TopOrderBookLevelAggregates {
    char ticker[core::constants::MAX_TICKER_LENGTH]{};
    std::array<LevelAggregate, core::constants::ORDER_BOOK_AGGREGATE_LEVELS> bid_level_aggregates;
    std::array<LevelAggregate, core::constants::ORDER_BOOK_AGGREGATE_LEVELS> ask_level_aggregates;

    TopOrderBookLevelAggregates(const char* ticker_str) {
        size_t len = strlen(ticker_str);
        assert(len > 0 && len < sizeof(ticker));
        memcpy(ticker, ticker_str, len);
        ticker[len + 1] = '\0';
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const TopOrderBookLevelAggregates& aggregates) {
        os << "ticker:" << aggregates.ticker << ",bids:[";
        for (const auto& level : aggregates.bid_level_aggregates) {
            os << level.quantity / core::constants::decimal_to_int_multiplier << "@"
               << level.price / core::constants::decimal_to_int_multiplier << ",";
        }
        os << "],asks:[";
        for (const auto& level : aggregates.ask_level_aggregates) {
            os << level.quantity / core::constants::decimal_to_int_multiplier << "@"
               << level.price / core::constants::decimal_to_int_multiplier << ",";
        }
        os << "]";
        return os;
    }

    void to_json(json& j) {
        j = json{{"ticker", ticker}, {"bids", json::array()}, {"asks", json::array()}};

        for (const auto& level : bid_level_aggregates) {
            j["bids"].push_back(
                json{{"price", level.price / core::constants::decimal_to_int_multiplier},
                     {"quantity", level.quantity / core::constants::decimal_to_int_multiplier}});
        }

        for (const auto& level : ask_level_aggregates) {
            j["asks"].push_back(
                json{{"price", level.price / core::constants::decimal_to_int_multiplier},
                     {"quantity", level.quantity / core::constants::decimal_to_int_multiplier}});
        }
    }

    static TopOrderBookLevelAggregates from_json(const json& j) {
        std::string ticker_str = j.at("ticker").get<std::string>();
        TopOrderBookLevelAggregates snapshot{ticker_str.c_str()};

        for (size_t i = 0; i < snapshot.bid_level_aggregates.size(); ++i) {
            snapshot.bid_level_aggregates[i].price =
                static_cast<int>(j.at("bids")[i].at("price").get<double>() *
                                 core::constants::decimal_to_int_multiplier);
            snapshot.bid_level_aggregates[i].quantity = j.at("bids")[i].at("quantity").get<int>() *
                                                        core::constants::decimal_to_int_multiplier;
        }

        for (size_t i = 0; i < snapshot.ask_level_aggregates.size(); ++i) {
            snapshot.ask_level_aggregates[i].price =
                static_cast<int>(j.at("asks")[i].at("price").get<double>() *
                                 core::constants::decimal_to_int_multiplier);
            snapshot.ask_level_aggregates[i].quantity = j.at("asks")[i].at("quantity").get<int>() *
                                                        core::constants::decimal_to_int_multiplier;
        }
        return snapshot;
    }
};

// typed ring buffer for communication with MDP
using OrderbookSnapshotRingBuffer =
    MpscSharedMemoryRingBuffer<TopOrderBookLevelAggregates,
                               core::constants::OrderbookSnapshotRingBufferCapacity>;

#endif // ELSA_FYP_CLIENT_SDK_ORDERBOOK_SNAPSHOT_H
