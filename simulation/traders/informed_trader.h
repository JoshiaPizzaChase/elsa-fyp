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
#include <format>

#include "config.h"
#include "fix_client.h"
#include "logger/logger.h"
#include "market_maker.h"   // For MarketDataHandler and OrderBookSnapshot
#include "oracle_service.h" // For OracleClient

namespace simulation {

// --- Informed FIX Client ---
// Handles execution for the informed trader, tracking inventory and fills.
class InformedFixClient : public FixClient {
public:
    InformedFixClient(const std::string& config_file,
                      std::shared_ptr<spdlog::logger> logger = nullptr)
        : FixClient(config_file), m_logger(std::move(logger)) {}

    // Sends an aggressive limit order meant to sweep the best bid/ask
    void send_snipe_order(const std::string& ticker, double price, double quantity, const std::string& side) {
        OrderSide order_side = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        int client_order_id = ++m_order_id_counter;

        if (m_logger) {
            m_logger->info(
                "[InformedFixClient] Submitting {} snipe order for {} | Px: {} | Qty: {} | ID: {}",
                side, ticker, price, quantity, client_order_id);
        }

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
            if (m_logger) {
                m_logger->info("[InformedFixClient] Snipe order filled! Ticker: {} | Side: {} | Qty: {} | New Inv: {}",
                               report.ticker, (report.side == OrderSide::BUY ? "BUY" : "SELL"),
                               report.filled_qty, m_inventory[report.ticker]);
            }
        }
    }

    void on_order_cancel_rejected(int client_order_id, const std::string& reason) override {
        if (m_logger) {
            m_logger->warn("[InformedTrader FIX] Cancel rejected for client_order_id {}: {}",
                           client_order_id, reason);
        }
    }

private:
    std::shared_ptr<spdlog::logger> m_logger;
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
                   double max_inventory = 1000.0, // Risk limit
                   std::shared_ptr<spdlog::logger> logger = nullptr)
        : m_md_handler(std::move(md_handler)),
          m_oracle(std::move(oracle_client)),
          m_fix_client(std::move(fix_client)),
          m_initial_inventory(initial_inventory),
          m_epsilon(epsilon),
          m_trade_qty(trade_qty),
          m_max_inventory(max_inventory),
          m_logger(std::move(logger)),
          m_running(false) {}

    ~InformedTrader() {
        stop();
    }

    void start(const std::vector<std::string>& tickers) {
        if (m_logger) {
            m_logger->info("[InformedTrader] Starting informed trader agent for {} tickers...",
                           tickers.size());
        }
        m_tickers = tickers;
        for (const auto& ticker : m_tickers) {
            m_fix_client->set_inventory(ticker, m_initial_inventory);
            if (m_logger) {
                m_logger->info("[InformedTrader] Seeded initial inventory for {}: {}", ticker,
                               m_initial_inventory);
            }
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void evaluate_and_trade(const std::string& ticker) {
        auto now = std::chrono::steady_clock::now();

        // Cooldown check to prevent spamming orders before the book updates
        if (now - m_last_trade_time[ticker] < std::chrono::milliseconds(500)) {
            if (m_logger) {
                m_logger->debug("[InformedTrader] [{}] In cooldown", ticker);
            }
            return;
        }

        // 1. Get current market state
        OrderBookSnapshot ob = m_md_handler->get_latest_orderbook(ticker);
        if (ob.best_bid_price == 0.0 || ob.best_ask_price == 0.0) {
            if (m_logger) {
                m_logger->debug("[InformedTrader] [{}] Orderbook not formed yet", ticker);
            }
            return; // Orderbook not formed yet
        }

        // 2. Get true fundamental price
        double true_price = m_oracle->get_true_price(ticker);
        double current_inventory = m_fix_client->get_inventory(ticker);

        static auto last_log_time = std::chrono::steady_clock::now();
        if (now - last_log_time > std::chrono::seconds(1)) {
            if (m_logger) {
                m_logger->info("[InformedTrader] [{}] TruePx: {} | BestBid: {} | BestAsk: {} | Inv: {}",
                               ticker, true_price, ob.best_bid_price, ob.best_ask_price,
                               current_inventory);
            }
            last_log_time = now;
        }

        // 3. Evaluate Sniping Opportunities (Adverse Selection)

        // Case A: Market is undervaluing the asset (True Price > Ask)
        if (true_price > ob.best_ask_price + m_epsilon) {
            // Buy from the market maker to push the price up
            if (current_inventory + m_trade_qty <= m_max_inventory) {
                if (m_logger) {
                    m_logger->info(
                        "[InformedTrader] [{}] Undervalued! True: {} > Ask: {} + eps({}). Sniping ASK!",
                        ticker, true_price, ob.best_ask_price, m_epsilon);
                }
                m_fix_client->send_snipe_order(ticker, ob.best_ask_price, m_trade_qty, "BUY");
                m_last_trade_time[ticker] = now;
            } else {
                if (m_logger) {
                    m_logger->warn("[InformedTrader] [{}] Skipping buy snipe, inventory limit reached.",
                                   ticker);
                }
            }
        }

        // Case B: Market is overvaluing the asset (True Price < Bid)
        else if (true_price < ob.best_bid_price - m_epsilon) {
            // Sell to the market maker to push the price down (no short selling)
            if (current_inventory - m_trade_qty >= 0.0) {
                if (m_logger) {
                    m_logger->info(
                        "[InformedTrader] [{}] Overvalued! True: {} < Bid: {} - eps({}). Sniping BID!",
                        ticker, true_price, ob.best_bid_price, m_epsilon);
                }
                m_fix_client->send_snipe_order(ticker, ob.best_bid_price, m_trade_qty, "SELL");
                m_last_trade_time[ticker] = now;
            } else {
                if (m_logger) {
                    m_logger->warn("[InformedTrader] [{}] Skipping sell snipe, zero inventory.",
                                   ticker);
                }
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
    std::shared_ptr<spdlog::logger> m_logger;

    std::vector<std::string> m_tickers;
    std::map<std::string, std::chrono::time_point<std::chrono::steady_clock>> m_last_trade_time;

    std::atomic<bool> m_running;
    std::thread m_worker_thread;
};

} // namespace simulation
