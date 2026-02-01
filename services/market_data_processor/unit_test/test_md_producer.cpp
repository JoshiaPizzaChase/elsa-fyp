#include "core/constants.h"
#include "core/orderbook_snapshot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <core/trade.h>
#include <iostream>
#include <random>
#include <thread>

constexpr int NUM_LEVELS = 50;
constexpr int PRICE_STEP = 1;
constexpr double MIN_BEST_BID = 51.0;
constexpr double TARGET_SPREAD = 2.0;
constexpr int MAX_TRADE_QUANTITY = 10000;
constexpr int MIN_TRADE_QUANTITY = 100;

void initializeOrderBook(TopOrderBookLevelAggregates& snapshot) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> bid_price_dist(MIN_BEST_BID, MIN_BEST_BID + 10.0);

    double best_bid = bid_price_dist(gen);
    double best_ask = best_bid + TARGET_SPREAD;

    snapshot.bid_level_aggregates[0].price = best_bid;
    snapshot.ask_level_aggregates[0].price = best_ask;

    for (int i = 1; i < NUM_LEVELS; i++) {
        snapshot.bid_level_aggregates[i].price = best_bid - (i * PRICE_STEP);
        snapshot.bid_level_aggregates[i].price =
            std::max(1, snapshot.bid_level_aggregates[i].price);
    }

    for (int i = 1; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].price = best_ask + (i * PRICE_STEP);
    }

    std::uniform_int_distribution<> qty_dist(MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);

    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].quantity = qty_dist(gen);
        snapshot.bid_level_aggregates[i].quantity = qty_dist(gen);
    }
}

void updateOrderBook(TopOrderBookLevelAggregates& snapshot) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_change_dist(-1.0, 1.0);
    std::uniform_int_distribution<> qty_dist(MIN_TRADE_QUANTITY, MAX_TRADE_QUANTITY);

    double price_change = price_change_dist(gen);
    double new_best_bid = snapshot.bid_level_aggregates[0].price + price_change;
    new_best_bid = std::max(MIN_BEST_BID, new_best_bid);

    double new_best_ask = new_best_bid + TARGET_SPREAD;

    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].price = new_best_ask + (i * PRICE_STEP);
        snapshot.ask_level_aggregates[i].quantity = qty_dist(gen);
    }

    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.bid_level_aggregates[i].price = new_best_bid - (i * PRICE_STEP);
        snapshot.bid_level_aggregates[i].price =
            std::max(1, snapshot.bid_level_aggregates[i].price);
        snapshot.bid_level_aggregates[i].quantity = qty_dist(gen);
    }
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
    double mid_price = (best_bid + best_ask) / 2.0;

    int price;
    if (is_taker_buyer) {
        // If taker is buyer, trade should be at or near ask price
        double price_offset = price_offset_dist(gen);
        double trade_price = best_ask + price_offset;
        price = static_cast<int>(std::round(trade_price));
    } else {
        // If taker is seller, trade should be at or near bid price
        double price_offset = price_offset_dist(gen);
        double trade_price = best_bid + price_offset;
        price = static_cast<int>(std::round(trade_price));
    }

    // Ensure price is positive
    price = std::max(1, price);

    int trade_id = trade_id_counter++;
    int taker_id = participant_dist(gen);
    int maker_id = participant_dist(gen);
    int taker_order_id = order_id_dist(gen);
    int maker_order_id = order_id_dist(gen);

    return Trade(snapshot.ticker, price, quantity, trade_id, taker_id, maker_id, taker_order_id,
                 maker_order_id, is_taker_buyer);
}

int main(int argc, char* argv[]) {
    try {
        OrderbookSnapshotRingBuffer orderbook_snapshot_buffer =
            OrderbookSnapshotRingBuffer::open_exist_shm(
                core::constants::ORDERBOOK_SNAPSHOT_SHM_FILE);
        TopOrderBookLevelAggregates snapshot{"APPL"};
        TradeRingBuffer trade_buffer =
            TradeRingBuffer::open_exist_shm(core::constants::TRADE_SHM_FILE);

        std::random_device rd;
        std::mt19937 gen(rd());
        int trade_id_counter = 1;

        initializeOrderBook(snapshot);

        if (orderbook_snapshot_buffer.try_push(snapshot)) {
            std::cout << "Initial snapshot pushed successfully!" << std::endl;
            printOrderBook(snapshot, "Initial");
        } else {
            std::cerr << "Failed to push initial snapshot!" << std::endl;
            return 1;
        }

        int update_count = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            updateOrderBook(snapshot);

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
                    std::cout << trade << std::endl;
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