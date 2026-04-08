#pragma once

#include <string>

namespace core::constants {
inline constexpr int MAX_TICKER_LENGTH = 5;
inline constexpr int MAX_USERNAME_LENGTH = 16;
inline constexpr int UUID_LENGTH = 37;
inline constexpr int ORDER_BOOK_AGGREGATE_LEVELS = 50;
inline constexpr int OrderbookSnapshotRingBufferCapacity = 1024;
inline constexpr int TradeRingBufferCapacity = 1024;
inline static std::string BUY_STR = "BUY";
inline static std::string SELL_STR = "SELL";
inline static std::string ORDERBOOK_SNAPSHOT_SHM_FILE = "os";
inline static std::string TRADE_SHM_FILE = "td";
inline constexpr double decimal_to_int_multiplier{100.0};
} // namespace core::constants
