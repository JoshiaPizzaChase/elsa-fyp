#pragma once

#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

#include <transport/websocket/websocket_client.h>
#include "fix_client.h"

namespace trading {

// --- Basic Data Structures ---

struct OrderBookSnapshot {
    double best_bid_price = 0.0;
    double best_bid_qty = 0.0;
    double best_ask_price = 0.0;
    double best_ask_qty = 0.0;
    
    double mid_price() const {
        if (best_bid_price > 0 && best_ask_price > 0) {
            return (best_bid_price + best_ask_price) / 2.0;
        }
        return 0.0;
    }
};

struct Trade {
    double price;
    double quantity;
    int64_t timestamp;
};

// --- Market Data Handler ---
// Handles incoming market data over Websockets and updates internal state safely.
class MarketDataHandler {
public:
    MarketDataHandler(std::shared_ptr<transport::WebsocketManagerClient> ws_client)
        : m_ws_client(std::move(ws_client)) {}

    // Simulates processing an incoming market data message
    void process_message(const std::string& ticker, const std::string& msg) {
        // In a real scenario, we would parse JSON/Binary here.
        // For demonstration, we simply lock and update structure.
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Mock update
        // m_orderbooks[ticker] = parsed_orderbook;
        // m_trades[ticker].push_back(parsed_trade);
        
        // Keep trade history bounded
        if (m_trades[ticker].size() > m_max_trades_window) {
            m_trades[ticker].pop_front();
        }
    }

    OrderBookSnapshot get_latest_orderbook(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_orderbooks[ticker];
    }

    std::vector<Trade> get_recent_trades(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<Trade>(m_trades[ticker].begin(), m_trades[ticker].end());
    }

    // Calculates rolling variance based on recent trade prices
    double calculate_variance(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& trades = m_trades[ticker];
        if (trades.size() < 2) return 0.0;

        double sum = 0.0;
        for (const auto& t : trades) sum += t.price;
        double mean = sum / trades.size();

        double sq_diff_sum = 0.0;
        for (const auto& t : trades) {
            double diff = t.price - mean;
            sq_diff_sum += diff * diff;
        }
        return sq_diff_sum / (trades.size() - 1);
    }

private:
    std::shared_ptr<transport::WebsocketManagerClient> m_ws_client;
    
    std::mutex m_mutex;
    std::map<std::string, std::deque<Trade>> m_trades;
    std::map<std::string, OrderBookSnapshot> m_orderbooks;
    
    const size_t m_max_trades_window = 1000; // N past trades
};


// --- FIX Client ---
// Handles sending and cancelling orders via FIX protocol
class MMFixClient : public FixClient {
public:
    MMFixClient(const std::string& config_file) : FixClient(config_file) {}

    void send_order(const std::string& ticker, double price, double quantity, const std::string& side) {
        OrderSide order_side = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        int client_order_id = ++m_order_id_counter;
        
        submit_limit_order(ticker, price, quantity, order_side, TimeInForce::GTC, client_order_id);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active_orders[ticker].push_back({client_order_id, order_side});
    }

    void cancel_all_orders(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& orders = m_active_orders[ticker];
        for (const auto& order : orders) {
            int cancel_id = ++m_order_id_counter;
            cancel_order(ticker, order.side, order.id, cancel_id);
        }
        orders.clear();
    }

    double get_inventory(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inventory[ticker];
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
        std::cerr << "[FIX] Order cancel rejected for ID " << client_order_id << ": " << reason << "\n";
    }

private:
    struct ActiveOrder {
        int id;
        OrderSide side;
    };
    std::mutex m_mutex;
    std::map<std::string, double> m_inventory;
    std::map<std::string, std::vector<ActiveOrder>> m_active_orders;
    std::atomic<int> m_order_id_counter{0};
};


// --- Market Maker Agent ---
/*
 * Avellaneda-Stoikov market maker agent.
 */
class MarketMaker {
public:
    MarketMaker(std::shared_ptr<MarketDataHandler> md_handler, 
                std::shared_ptr<MMFixClient> fix_client,
                double gamma = 0.1, 
                double k = 1.5,
                double terminal_time = 1.0)
        : m_md_handler(std::move(md_handler)),
          m_fix_client(std::move(fix_client)),
          m_gamma(gamma),
          m_k(k),
          m_terminal_time(terminal_time),
          m_running(false) {}

    ~MarketMaker() {
        stop();
    }

    void start(const std::vector<std::string>& tickers) {
        m_tickers = tickers;
        m_running = true;
        m_worker_thread = std::thread(&MarketMaker::run_loop, this);
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
        // Market making loop running periodically
        while (m_running) {
            for (const auto& ticker : m_tickers) {
                make_market(ticker);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz quoting
        }
    }

    void make_market(const std::string& ticker) {
        // 1. Get latest orderbook and mid price
        OrderBookSnapshot ob = m_md_handler->get_latest_orderbook(ticker);
        double mid_price = ob.mid_price();
        if (mid_price == 0.0) return; // Not enough data yet

        // 2. Get rolling N volatility (variance)
        double variance = m_md_handler->calculate_variance(ticker);
        if (variance == 0.0) {
            variance = 0.01; // Default fallback to avoid 0 spread
        }

        // 3. Get current inventory (q)
        double q = m_fix_client->get_inventory(ticker);

        // Current time t is normalized. For continuous AS, let's assume a static time_left or compute dynamically.
        double time_left = m_terminal_time; // Simplified T - t

        // 4. Calculate Reservation Price (r)
        // r(s, t) = s - q * gamma * sigma^2 * (T - t)
        double reservation_price = mid_price - (q * m_gamma * variance * time_left);

        // 5. Calculate Optimal Spread
        // spread = gamma * sigma^2 * (T - t) + (2 / gamma) * ln(1 + gamma / k)
        double spread = (m_gamma * variance * time_left) + 
                        ((2.0 / m_gamma) * std::log(1.0 + (m_gamma / m_k)));

        // 6. Calculate Bid and Ask Quotes
        double optimal_bid = reservation_price - (spread / 2.0);
        double optimal_ask = reservation_price + (spread / 2.0);

        // 7. Execute Strategy
        place_quotes(ticker, optimal_bid, optimal_ask);
    }

    void place_quotes(const std::string& ticker, double bid_price, double ask_price) {
        // Simplistic approach: cancel all open orders and place new ones.
        // In a real system, you would delta-check to avoid spamming the exchange.
        m_fix_client->cancel_all_orders(ticker);
        
        // Standard order quantity
        double order_qty = 1.0; 
        
        m_fix_client->send_order(ticker, bid_price, order_qty, "BUY");
        m_fix_client->send_order(ticker, ask_price, order_qty, "SELL");
    }

private:
    std::shared_ptr<MarketDataHandler> m_md_handler;
    std::shared_ptr<MMFixClient> m_fix_client;
    
    // Model parameters
    double m_gamma;         // Constant gamma risk parameter
    double m_k;             // Order book liquidity parameter
    double m_terminal_time; // T - t (time remaining)

    std::vector<std::string> m_tickers;
    
    std::atomic<bool> m_running;
    std::thread m_worker_thread;
};

} // namespace trading