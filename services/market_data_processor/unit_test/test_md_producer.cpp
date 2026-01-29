#include "core/orderbook_snapshot.h"
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>

constexpr int NUM_LEVELS = 50;
constexpr int PRICE_STEP = 1;
constexpr double MIN_BEST_BID = 51.0;
constexpr double TARGET_SPREAD = 2.0;

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
        snapshot.bid_level_aggregates[i].price = std::max(1, snapshot.bid_level_aggregates[i].price);
    }

    for (int i = 1; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].price = best_ask + (i * PRICE_STEP);
    }

    std::uniform_int_distribution<> qty_dist(100, 10000);

    for (int i = 0; i < NUM_LEVELS; i++) {
        snapshot.ask_level_aggregates[i].quantity = qty_dist(gen);
        snapshot.bid_level_aggregates[i].quantity = qty_dist(gen);
    }
}

void updateOrderBook(TopOrderBookLevelAggregates& snapshot) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_change_dist(-1.0, 1.0);
    std::uniform_int_distribution<> qty_dist(100, 10000);

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
        snapshot.bid_level_aggregates[i].price = std::max(1, snapshot.bid_level_aggregates[i].price);
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
    std::cout << "Spread: " << (snapshot.ask_level_aggregates[0].price - snapshot.bid_level_aggregates[0].price)
              << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        OrderbookSnapshotRingBuffer buffer = OrderbookSnapshotRingBuffer::open_exist_shm(ORDERBOOK_SNAPSHOT_SHM_FILE);
        TopOrderBookLevelAggregates snapshot("APPL");
        initializeOrderBook(snapshot);

        if (buffer.try_push(snapshot)) {
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

            if (buffer.try_push(snapshot)) {
                update_count++;
                std::cout << "\nUpdate #" << update_count << " pushed successfully!" << std::endl;
                printOrderBook(snapshot, "Update " + std::to_string(update_count));
            } else {
                std::cerr << "Failed to push update #" << update_count << "!" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}