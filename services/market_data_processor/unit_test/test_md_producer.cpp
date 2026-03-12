#include "core/constants.h"
#include "core/orderbook_snapshot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <core/trade.h>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <deque>
#include <numeric>
#include <cstring>
#include <cassert>

constexpr int NUM_LEVELS = 50;
constexpr int PRICE_STEP = 1;
constexpr double MIN_BEST_BID = 51.0;
constexpr double MAX_BEST_BID = 200.0;  // Add upper bound
constexpr double TARGET_SPREAD = 2.0;
constexpr int MAX_TRADE_QUANTITY = 10000;
constexpr int MIN_TRADE_QUANTITY = 100;

// New constants for price movement
constexpr double MAX_PRICE_CHANGE_PER_UPDATE = 0.25;  // Max price change per second
constexpr double VOLATILITY_FACTOR = 0.1;  // Controls price volatility
constexpr int TREND_WINDOW_SIZE = 10;  // Number of updates to consider for trend
constexpr double MEAN_REVERSION_STRENGTH = 0.01;  // How strongly to revert to mean

class PriceSimulator {
private:
    std::mt19937 gen;
    std::normal_distribution<> price_change_dist;
    std::deque<double> price_history;
    double current_trend = 0.0;
    double long_term_mean = 100.0;  // Long-term mean price
    double current_price;

public:
    PriceSimulator(double initial_price)
        : gen(std::random_device{}()),
          price_change_dist(0.0, VOLATILITY_FACTOR),
          current_price(initial_price) {
        price_history.push_back(initial_price);
    }

    double getNextPrice() {
        // Calculate mean reversion component
        double mean_reversion = 0.0;
        if (!price_history.empty()) {
            double avg_price = std::accumulate(price_history.begin(), price_history.end(), 0.0)
                               / price_history.size();
            mean_reversion = (long_term_mean - avg_price) * MEAN_REVERSION_STRENGTH;
        }

        // Calculate trend component
        double trend_component = current_trend * 0.5;

        // Random walk component (using normal distribution for smoother changes)
        double random_component = price_change_dist(gen);

        // Combine components
        double price_change = mean_reversion + trend_component + random_component;

        // Limit maximum change
        price_change = std::clamp(price_change,
                                  -MAX_PRICE_CHANGE_PER_UPDATE,
                                  MAX_PRICE_CHANGE_PER_UPDATE);

        // Update price
        current_price += price_change;

        // Ensure price stays within bounds
        current_price = std::clamp(current_price, MIN_BEST_BID, MAX_BEST_BID);

        // Update price history
        price_history.push_back(current_price);
        if (price_history.size() > TREND_WINDOW_SIZE) {
            price_history.pop_front();
        }

        // Update trend based on recent movements
        if (price_history.size() >= 2) {
            double recent_change = current_price - price_history[price_history.size() - 2];
            current_trend = current_trend * 0.7 + recent_change * 0.3;  // EMA of changes
        }

        return current_price;
    }

    void setLongTermMean(double mean) {
        long_term_mean = mean;
    }
};

void initializeOrderBook(TopOrderBookLevelAggregates& snapshot) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> bid_price_dist(MIN_BEST_BID, MIN_BEST_BID + 10.0);

    double best_bid = bid_price_dist(gen);
    double best_ask = best_bid + TARGET_SPREAD;

    snapshot.bid_level_aggregates[0].price = static_cast<int>(std::round(best_bid));
    snapshot.ask_level_aggregates[0].price = static_cast<int>(std::round(best_ask));

    for (int i = 1; i < NUM_LEVELS; i++) {
        snapshot.bid_level_aggregates[i].price =
            static_cast<int>(std::round(best_bid - (i * PRICE_STEP)));
        snapshot.bid_level_aggregates[i].price =
            std::max(1, snapshot.bid_level_aggregates[i].price);
    }

    for (int i = 1; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].price =
            static_cast<int>(std::round(best_ask + (i * PRICE_STEP)));
    }

    std::uniform_int_distribution<> qty_dist(MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);

    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].quantity = qty_dist(gen);
        snapshot.bid_level_aggregates[i].quantity = qty_dist(gen);
    }
}

void updateOrderBook(TopOrderBookLevelAggregates& snapshot, PriceSimulator& price_sim) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> qty_dist(MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);

    // Get new price from simulator
    double new_best_bid = price_sim.getNextPrice();
    double new_best_ask = new_best_bid + TARGET_SPREAD;

    // Update quantities with some correlation to price movement
    // This makes the simulation more realistic
    std::normal_distribution<> qty_noise_dist(0.0, 0.2);  // 20% standard deviation

    // Update ask levels
    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].price =
            static_cast<int>(std::round(new_best_ask + (i * PRICE_STEP)));

        // Add some randomness to quantities, but keep them within bounds
        double noise_factor = 1.0 + qty_noise_dist(gen);
        int base_qty = qty_dist(gen);
        int new_qty = static_cast<int>(base_qty * noise_factor);
        snapshot.ask_level_aggregates[i].quantity =
            std::clamp(new_qty, MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);
    }

    // Update bid levels
    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.bid_level_aggregates[i].price =
            static_cast<int>(std::round(new_best_bid - (i * PRICE_STEP)));
        snapshot.bid_level_aggregates[i].price =
            std::max(1, snapshot.bid_level_aggregates[i].price);

        // Add some randomness to quantities
        double noise_factor = 1.0 + qty_noise_dist(gen);
        int base_qty = qty_dist(gen);
        int new_qty = static_cast<int>(base_qty * noise_factor);
        snapshot.bid_level_aggregates[i].quantity =
            std::clamp(new_qty, MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);
    }

    auto now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    snapshot.create_timestamp = now_ts_ms;
}

void printOrderBook(const TopOrderBookLevelAggregates& snapshot, const std::string& timestamp) {
    std::cout << "\n=== Order Book Snapshot (" << timestamp << ") ===" << std::endl;
    std::cout << "ASKS:" << std::endl;
    for (int i = 0; i < NUM_LEVELS; i++) {
        std::cout << "  Level " << i << ": Price=" << snapshot.ask_level_aggregates[i].price
                  << ", Qty=" << snapshot.ask_level_aggregates[i].quantity << std::endl;
    }

    std::cout << "\nBIDS:" << std::endl;
    for (int i = 0; i < NUM_LEVELS; i++) {
        std::cout << "  Level " << i << ": Price=" << snapshot.bid_level_aggregates[i].price
                  << ", Qty=" << snapshot.bid_level_aggregates[i].quantity << std::endl;
    }
    std::cout << "Spread: "
              << (snapshot.ask_level_aggregates[0].price - snapshot.bid_level_aggregates[0].price)
              << std::endl;
}

// Helper function to generate random trades
Trade generateRandomTrade(const TopOrderBookLevelAggregates& snapshot, std::mt19937& gen,
                          int& trade_id_counter) {
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> qty_dist(MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);
    std::uniform_real_distribution<> price_offset_dist(-0.5, 0.5);
    std::uniform_int_distribution<> participant_dist(1000, 9999);
    std::uniform_int_distribution<> order_id_dist(10000, 99999);

    bool is_taker_buyer = side_dist(gen) == 0;
    int quantity = qty_dist(gen);

    double best_bid = snapshot.bid_level_aggregates[0].price;
    double best_ask = snapshot.ask_level_aggregates[0].price;

    int price;
    if (is_taker_buyer) {
        double price_offset = price_offset_dist(gen);
        double trade_price = best_ask + price_offset;
        price = static_cast<int>(std::round(trade_price));
    } else {
        double price_offset = price_offset_dist(gen);
        double trade_price = best_bid + price_offset;
        price = static_cast<int>(std::round(trade_price));
    }

    price = std::max(1, price);

    int trade_id = trade_id_counter++;
    int taker_id = participant_dist(gen);
    int maker_id = participant_dist(gen);
    int taker_order_id = order_id_dist(gen);
    int maker_order_id = order_id_dist(gen);
    auto now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Use the Trade constructor
    return Trade(snapshot.ticker, price, quantity, trade_id, taker_id, maker_id,
                 taker_order_id, maker_order_id, is_taker_buyer, now_ts_ms);
}

int main(int argc, char* argv[]) {
    try {
        auto now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        OrderbookSnapshotRingBuffer orderbook_snapshot_buffer =
            OrderbookSnapshotRingBuffer::open_exist_shm(
                core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE);

        // Use the TopOrderBookLevelAggregates constructor
        TopOrderBookLevelAggregates snapshot("AAPL", now_ts_ms);

        TradeRingBuffer trade_buffer =
            TradeRingBuffer::open_exist_shm(core::constants::TRADE_SHM_FILE);

        std::random_device rd;
        std::mt19937 gen(rd());
        int trade_id_counter = 1;

        // Initialize order book
        initializeOrderBook(snapshot);

        // Initialize price simulator with initial bid price
        PriceSimulator price_sim(snapshot.bid_level_aggregates[0].price);

        if (orderbook_snapshot_buffer.try_push(snapshot)) {
            std::cout << "Initial snapshot pushed successfully!" << std::endl;
            printOrderBook(snapshot, "Initial");
        } else {
            std::cerr << "Failed to push initial snapshot!" << std::endl;
            return 1;
        }

        int update_count = 0;
        auto last_trend_update = std::chrono::steady_clock::now();

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Occasionally adjust the long-term mean to simulate market regime changes
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - last_trend_update).count() >= 5) {
                // Every 5 minutes, possibly shift the mean
                std::uniform_real_distribution<> mean_shift_dist(-5.0, 5.0);
                double new_mean = snapshot.bid_level_aggregates[0].price + mean_shift_dist(gen);
                price_sim.setLongTermMean(std::clamp(new_mean, MIN_BEST_BID, MAX_BEST_BID));
                last_trend_update = now;
            }

            // Update order book with new simulated price
            updateOrderBook(snapshot, price_sim);

            if (orderbook_snapshot_buffer.try_push(snapshot)) {
                update_count++;
                std::cout << "\nUpdate #" << update_count << " pushed successfully!" << std::endl;
                printOrderBook(snapshot, "Update " + std::to_string(update_count));
            } else {
                std::cerr << "Failed to push update #" << update_count << "!" << std::endl;
            }

            // Generate random trades around latest bid/ask
            std::uniform_int_distribution<> trade_count_dist(1, 5);
            int num_trades = trade_count_dist(gen);
            for (int i = 0; i < num_trades; i++) {
                Trade trade = generateRandomTrade(snapshot, gen, trade_id_counter);

                if (trade_buffer.try_push(trade)) {
                    std::cout << "Trade executed: ID=" << trade.trade_id
                              << ", Price=" << trade.price
                              << ", Qty=" << trade.quantity
                              << ", Side=" << (trade.is_taker_buyer ? "BUY" : "SELL")
                              << std::endl;
                } else {
                    std::cerr << "Failed to push trade to buffer!" << std::endl;
                }

                // Small delay between trades
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}