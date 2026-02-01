#ifndef ELSA_FYP_TRADE_H
#define ELSA_FYP_TRADE_H

#include "constants.h"
#include "inter_process/mpsc_shared_memory_ring_buffer.h"

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

    Trade(const char* ticker_str, int price, int quantity, int trade_id, int taker_id, int maker_id,
          int taker_order_id, int maker_order_id, bool is_taker_buyer)
        : price{price}, quantity{quantity}, trade_id{trade_id}, taker_id{taker_id},
          maker_id{maker_id}, taker_order_id{taker_order_id}, maker_order_id{maker_order_id},
          is_taker_buyer{is_taker_buyer} {
        size_t len = strlen(ticker_str);
        assert(len > 0 && len < sizeof(ticker));
        memcpy(ticker, ticker_str, len);
        ticker[len + 1] = '\0';
    }
};

// typed ring buffer for communication with MDP
static std::string TRADE_SHM_FILE = "trade";
using TradeRingBuffer = MpscSharedMemoryRingBuffer<Trade, core::constants::TradeRingBufferCapacity>;

#endif // ELSA_FYP_TRADE_H
