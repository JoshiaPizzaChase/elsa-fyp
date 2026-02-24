#ifndef ELSA_FYP_TRADE_H
#define ELSA_FYP_TRADE_H

#include "constants.h"
#include "inter_process/mpsc_shared_memory_ring_buffer.h"
#include "nlohmann/json.hpp"
#include <assert.h>

using json = nlohmann::json;

struct Trade {
    char ticker[core::constants::MAX_TICKER_LENGTH]{};
    int price{0};
    int quantity{0};

    int trade_id{0};
    int taker_id{0};
    int maker_id{0};
    int taker_order_id{0};
    int maker_order_id{0};
    bool is_taker_buyer{false};
    uint64_t create_timestamp;

    Trade(const char* ticker_str, int price, int quantity, int trade_id, int taker_id, int maker_id,
          int taker_order_id, int maker_order_id, bool is_taker_buyer, uint64_t create_timestamp)
        : price{price}, quantity{quantity}, trade_id{trade_id}, taker_id{taker_id},
          maker_id{maker_id}, taker_order_id{taker_order_id}, maker_order_id{maker_order_id},
          is_taker_buyer{is_taker_buyer}, create_timestamp(create_timestamp) {
        size_t len = strlen(ticker_str);
        assert(len > 0 && len < sizeof(ticker));
        memcpy(ticker, ticker_str, len);
        ticker[len + 1] = '\0';
    }

    friend std::ostream& operator<<(std::ostream& os, const Trade& trade) {
        std::string side_str = trade.is_taker_buyer ? " BUY" : " SELL";
        os << "ticker:" << trade.ticker << side_str
           << trade.quantity / core::constants::decimal_to_int_multiplier << "@"
           << trade.price / core::constants::decimal_to_int_multiplier
           << ",trade_id:" << trade.trade_id << ",taker_id:" << trade.taker_id
           << ",maker_id:" << trade.maker_id << ",taker_order_id:" << trade.taker_order_id
           << ",maker_order_id:" << trade.maker_order_id
           << ",create_timestamp:" << trade.create_timestamp;

        return os;
    }

    void to_json(json& j) {
        std::string side_str =
            is_taker_buyer ? core::constants::BUY_STR : core::constants::SELL_STR;
        j = json{{"ticker", ticker},
                 {"trade_id", trade_id},
                 {"taker_side", side_str},
                 {"price", price / core::constants::decimal_to_int_multiplier},
                 {"quantity", quantity / core::constants::decimal_to_int_multiplier},
                 {"taker_id", taker_id},
                 {"maker_id", maker_id},
                 {"taker_order_id", taker_order_id},
                 {"maker_order_id", maker_order_id},
                 {"create_timestamp", create_timestamp}};
    }

    static Trade from_json(const json& j) {
        std::string ticker_str = j.at("ticker").get<std::string>();
        bool is_taker_buy = j.at("taker_side").get<std::string>().compare(core::constants::BUY_STR);
        int price = j.at("price").get<double>() * core::constants::decimal_to_int_multiplier;
        int quantity = j.at("quantity").get<double>() * core::constants::decimal_to_int_multiplier;
        int trade_id = j.at("trade_id").get<int>();
        int taker_id = j.at("taker_id").get<int>();
        int maker_id = j.at("maker_id").get<int>();
        int taker_order_id = j.at("taker_order_id").get<int>();
        int maker_order_id = j.at("maker_order_id").get<int>();
        int create_timestamp = j.at("create_timestamp").get<int>();
        return Trade{ticker_str.c_str(), price,           quantity,       trade_id,
                     taker_id,           maker_id,        taker_order_id, maker_order_id,
                     is_taker_buy,       create_timestamp};
    }
};

// typed ring buffer for communication with MDP
using TradeRingBuffer = MpscSharedMemoryRingBuffer<Trade, core::constants::TradeRingBufferCapacity>;

#endif // ELSA_FYP_TRADE_H
