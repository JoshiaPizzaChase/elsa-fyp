#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>

#include "fix_client.h"
#include "market_maker.h"   // For MarketDataHandler and OrderBookSnapshot
#include "oracle_service.h" // For OracleClient

namespace simulation {

// --- Informed FIX Client ---
// Handles execution for the informed trader, tracking inventory and fills.
class InformedFixClient : public FixClient {
public:
    InformedFixClient(const std::string& config_file) : FixClient(config_file) {}

    // Sends an aggressive limit order meant to sweep the best bid/ask
    void send_snipe_order(const std::string& ticker, double price, double quantity, const std::string& side) {
        OrderSide order_side = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        int client_order_id = ++m_order_id_counter;
        
        // Using IMMEDIATE_OR_CANCEL if available in your TimeInForce enum, otherwise GTC. 
        // We'll use GTC for safety but ideally this should be IOC.
        submit_limit_order(ticker, price, quantity, order_side, TimeInForce::GTC, client_order_id);
    }

    double get_inventory(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inventory[ticker];
    }

    void set_inventory(const std::string& ticker, double inventory) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inventory[ticker] = inventory;
    }

protected:
    void on_order_update(const ExecutionReport& report) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (report.status == OrderStatus::FILLED || report.status == OrderStatus::PARTIALLY_FILLED) {
            if (report.side == OrderSide::BUY) {
                m_inventory[report.ticker] += report.filled_qty;
            } else {
                m_inventory[report.ticker] -= report.filled_qty;
            }
        }
    }

    void on_order_cancel_rejected(int client_order_id, const std::string& reason) override {
        std::cerr << "[InformedTrader FIX] Cancel rejected: " << reason << "\n";
    }

private:
    std::mutex m_mutex;
    std::map<std::string, double> m_inventory;
    std::atomic<int> m_order_id_counter{0};
};

// --- Informed Trader Agent ---
/*
 * Informed Trader Bot:
 * Knows the "true" fundamental price V(t) from the OracleService.
 * Snipes the market makers when their quotes lag behind the true price,
 * creating adverse selection and driving price discovery.
 */
class InformedTrader {
public:
    InformedTrader(std::shared_ptr<MarketDataHandler> md_handler,
                   std::shared_ptr<OracleClient> oracle_client,
                   std::shared_ptr<InformedFixClient> fix_client,
                   double initial_inventory = 100.0,
                   double epsilon = 0.05,       // Minimum mispricing to act on
                   double trade_qty = 10.0,     // Size of the sniping order
                   double max_inventory = 1000.0) // Risk limit
        : m_md_handler(std::move(md_handler)),
          m_oracle(std::move(oracle_client)),
          m_fix_client(std::move(fix_client)),
          m_initial_inventory(initial_inventory),
          m_epsilon(epsilon),
          m_trade_qty(trade_qty),
          m_max_inventory(max_inventory),
          m_running(false) {}

    ~InformedTrader() {
        stop();
    }

    void start(const std::vector<std::string>& tickers) {
        m_tickers = tickers;
        for (const auto& ticker : m_tickers) {
            m_fix_client->set_inventory(ticker, m_initial_inventory);
        }
        m_running = true;
        m_worker_thread = std::thread(&InformedTrader::run_loop, this);
    }

    void stop() {
        if (m_running) {
            m_running = false;
            if (m_worker_thread.joinable()) {
                m_worker_thread.join();
            }
        }
    }

private:
    void run_loop() {
        while (m_running) {
            for (const auto& ticker : m_tickers) {
                evaluate_and_trade(ticker);
            }
            // Check for mispricings frequently
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void evaluate_and_trade(const std::string& ticker) {
        auto now = std::chrono::steady_clock::now();
        
        // Cooldown check to prevent spamming orders before the book updates
        if (now - m_last_trade_time[ticker] < std::chrono::milliseconds(500)) {
            return;
        }

        // 1. Get current market state
        OrderBookSnapshot ob = m_md_handler->get_latest_orderbook(ticker);
        if (ob.best_bid_price == 0.0 || ob.best_ask_price == 0.0) {
            return; // Orderbook not formed yet
        }

        // 2. Get true fundamental price
        double true_price = m_oracle->get_true_price(ticker);
        double current_inventory = m_fix_client->get_inventory(ticker);

        // 3. Evaluate Sniping Opportunities (Adverse Selection)
        
        // Case A: Market is undervaluing the asset (True Price > Ask)
        if (true_price > ob.best_ask_price + m_epsilon) {
            // Buy from the market maker to push the price up
            if (current_inventory + m_trade_qty <= m_max_inventory) {
                m_fix_client->send_snipe_order(ticker, ob.best_ask_price, m_trade_qty, "BUY");
                m_last_trade_time[ticker] = now;
                std::cout << "[Informed] Sniping ASK! True: " << true_price << " Ask: " << ob.best_ask_price << "\n";
            }
        }
        
        // Case B: Market is overvaluing the asset (True Price < Bid)
        else if (true_price < ob.best_bid_price - m_epsilon) {
            // Sell to the market maker to push the price down (no short selling)
            if (current_inventory - m_trade_qty >= 0.0) {
                m_fix_client->send_snipe_order(ticker, ob.best_bid_price, m_trade_qty, "SELL");
                m_last_trade_time[ticker] = now;
                std::cout << "[Informed] Sniping BID! True: " << true_price << " Bid: " << ob.best_bid_price << "\n";
            }
        }
    }

    std::shared_ptr<MarketDataHandler> m_md_handler;
    std::shared_ptr<OracleClient> m_oracle;
    std::shared_ptr<InformedFixClient> m_fix_client;

    // Sniping parameters
    double m_initial_inventory;
    double m_epsilon;
    double m_trade_qty;
    double m_max_inventory;

    std::vector<std::string> m_tickers;
    std::map<std::string, std::chrono::time_point<std::chrono::steady_clock>> m_last_trade_time;

    std::atomic<bool> m_running;
    std::thread m_worker_thread;
};

} // namespace simulation