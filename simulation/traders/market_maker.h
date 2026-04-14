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
#include <chrono>
#include <nlohmann/json.hpp>

#include <transport/websocket/websocket_client.h>
#include "fix_client.h"

/*
 * This part mostly vibe coded, although its initial attempt was quite inefficient.
 * I accept it, mostly because I provided skeleton code and structure, checked it through, and already learnt similar stuff in IMC Prosperity 3.
 */
namespace simulation {

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

// Handles incoming market data over Websockets and updates internal state safely.
class MarketDataHandler {
public:
    MarketDataHandler(std::shared_ptr<transport::WebsocketManagerClient> ws_client)
        : m_ws_client(std::move(ws_client)), m_running(false) {}

    ~MarketDataHandler() {
        stop();
    }

    void start() {
        if (!m_running) {
            std::cout << "[MarketDataHandler] Starting consumer loop...\n";
            m_running = true;
            m_thread = std::thread(&MarketDataHandler::consume_loop, this);
        }
    }

    void stop() {
        if (m_running) {
            m_running = false;
            if (m_thread.joinable()) {
                m_thread.join();
            }
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
    // Use a sliding window approach (my idea lol)
    double calculate_variance(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_trade_stats[ticker].get_variance();
    }

private:
    void consume_loop() {
        while (m_running) {
            auto msg_opt = m_ws_client->dequeue_message(0);
            if (msg_opt) {
                process_message(*msg_opt);
            }
        }
    }

    void process_message(const std::string& msg) {
        try {
            auto j = nlohmann::json::parse(msg);
            if (j.contains("ticker")) {
                std::string ticker = j["ticker"];
                if (j.contains("trade_id")) {
                    Trade t;
                    t.price = j["price"].get<double>();
                    t.quantity = j["quantity"].get<double>();
                    t.timestamp = j.contains("create_timestamp") ? j["create_timestamp"].get<int64_t>() : 0;

                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_trades[ticker].push_back(t);
                    m_trade_stats[ticker].add_trade(t.price);

                    if (m_trades[ticker].size() > m_max_trades_window) {
                        double old_price = m_trades[ticker].front().price;
                        m_trade_stats[ticker].remove_trade(old_price);
                        m_trades[ticker].pop_front();
                    }
                } else {
                    OrderBookSnapshot ob;
                    if (j.contains("bids") && !j["bids"].empty()) {
                        ob.best_bid_price = j["bids"][0]["price"].get<double>();
                        ob.best_bid_qty = j["bids"][0]["quantity"].get<double>();
                    }
                    if (j.contains("asks") && !j["asks"].empty()) {
                        ob.best_ask_price = j["asks"][0]["price"].get<double>();
                        ob.best_ask_qty = j["asks"][0]["quantity"].get<double>();
                    }

                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_orderbooks[ticker] = ob;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing market data: " << e.what() << "\n";
        }
    }

    std::shared_ptr<transport::WebsocketManagerClient> m_ws_client;

    struct TradeStats {
        double sum_x = 0.0;
        double sum_x2 = 0.0;
        size_t count = 0;

        void add_trade(double price) {
            sum_x += price;
            sum_x2 += price * price;
            count++;
        }

        void remove_trade(double price) {
            sum_x -= price;
            sum_x2 -= price * price;
            count--;
        }

        double get_variance() const {
            if (count < 2) return 0.0;
            double var = (sum_x2 - (sum_x * sum_x) / count) / (count - 1);
            return var > 0.0 ? var : 0.0;
        }
    };

    std::mutex m_mutex;
    std::map<std::string, std::deque<Trade>> m_trades;
    std::map<std::string, TradeStats> m_trade_stats;
    std::map<std::string, OrderBookSnapshot> m_orderbooks;

    std::atomic<bool> m_running;
    std::thread m_thread;

    const size_t m_max_trades_window = 1000; // N past trades
};


class MMFixClient : public FixClient {
public:
    MMFixClient(const std::string& config_file) : FixClient(config_file) {}

    void send_order(const std::string& ticker, double price, double quantity, const std::string& side) {
        if (!is_connected()) {
            std::cout << "[MMFixClient] Not connected...\n";
            return;
        }
        OrderSide order_side = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        int client_order_id = ++m_order_id_counter;

        std::cout << "[MMFixClient] Submitting " << side << " order for " << ticker
                  << " | Px: " << price << " | Qty: " << quantity
                  << " | ID: " << client_order_id << "\n";

        submit_limit_order(ticker, price, quantity, order_side, TimeInForce::GTC, client_order_id);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_active_orders[ticker].push_back({client_order_id, order_side});
    }

    void cancel_all_orders(const std::string& ticker) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& orders = m_active_orders[ticker];
        if (!orders.empty()) {
            std::cout << "[MMFixClient] Cancelling " << orders.size() << " active orders for " << ticker << "\n";
            for (const auto& order : orders) {
                int cancel_id = ++m_order_id_counter;
                cancel_order(ticker, order.side, order.id, cancel_id);
            }
            orders.clear();
        }
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
        std::cout << "[MMFixClient] Order update...\n";
        std::lock_guard<std::mutex> lock(m_mutex);
        if (report.status == OrderStatus::FILLED || report.status == OrderStatus::PARTIALLY_FILLED) {
            if (report.side == OrderSide::BUY) {
                m_inventory[report.ticker] += report.filled_qty;
            } else {
                m_inventory[report.ticker] -= report.filled_qty;
            }
            std::cout << "[MMFixClient] Order filled! Ticker: " << report.ticker
                      << " | Side: " << (report.side == OrderSide::BUY ? "BUY" : "SELL")
                      << " | Qty: " << report.filled_qty
                      << " | New Inv: " << m_inventory[report.ticker] << "\n";
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


/*
 * Avellaneda-Stoikov market maker agent.
 */
class MarketMaker {
public:
    MarketMaker(std::shared_ptr<MarketDataHandler> md_handler,
                std::shared_ptr<MMFixClient> fix_client,
                double initial_inventory = 100.0,
                double lot_size = 1.0,
                double gamma = 0.1,
                double k = 1.5,
                double terminal_time = 1.0)
        : m_md_handler(std::move(md_handler)),
          m_fix_client(std::move(fix_client)),
          m_initial_inventory(initial_inventory),
          m_lot_size(lot_size),
          m_gamma(gamma),
          m_k(k),
          m_terminal_time(terminal_time),
          m_running(false) {}

    ~MarketMaker() {
        stop();
    }

    void start(const std::vector<std::string>& tickers) {
        std::cout << "[MarketMaker] Starting market maker agent for " << tickers.size() << " tickers...\n";
        m_tickers = tickers;
        for (const auto& ticker : m_tickers) {
            m_fix_client->set_inventory(ticker, m_initial_inventory);
            std::cout << "[MarketMaker] Seeded initial inventory for " << ticker << ": " << m_initial_inventory << "\n";
        }
        m_running = true;
        m_start_time = std::chrono::steady_clock::now();
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
        while (m_running) {
            for (const auto& ticker : m_tickers) {
                make_market(ticker);
            }
            // To avoid spamming the exchange, sleep between quotes
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 50Hz quoting
        }
    }

    void make_market(const std::string& ticker) {
        OrderBookSnapshot ob = m_md_handler->get_latest_orderbook(ticker);
        double mid_price = ob.mid_price();
        if (mid_price == 0.0) {
            mid_price =  100.0;
        }; // Not enough data yet

        double variance = m_md_handler->calculate_variance(ticker);
        if (variance == 0.0) {
            variance = 0.01; // Default fallback to avoid 0 spread
        }

        double q = m_fix_client->get_inventory(ticker);
        double q_centered = q - m_initial_inventory;

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - m_start_time;
        double time_left = std::max(0.0, (m_terminal_time - elapsed.count()) / m_terminal_time);

        // Calculate Reservation Price (r)
        // Treat initial inventory as target inventory to prevent massive negative price drift
        double reservation_price = mid_price - (q_centered * m_gamma * variance * time_left);

        // Calculate Optimal Spread
        // spread = gamma * sigma^2 * (T - t) + (2 / gamma) * ln(1 + gamma / k)
        double spread = (m_gamma * variance * time_left) +
                        ((2.0 / m_gamma) * std::log(1.0 + (m_gamma / m_k)));

        double optimal_bid = reservation_price - (spread / 2.0);
        double optimal_ask = reservation_price + (spread / 2.0);

        std::cout << "[MarketMaker] [" << ticker << "] Mid: " << mid_price
                  << " | Var: " << variance << " | Inv: " << q
                  << " | ResPx: " << reservation_price << " | Spread: " << spread << "\n";

        place_quotes(ticker, optimal_bid, optimal_ask, spread);
    }

    void place_quotes(const std::string& ticker, double bid_price, double ask_price, double spread) {
        // This may spam the exchange with constant cancels? Although we do have the sleeps.
        m_fix_client->cancel_all_orders(ticker);

        int num_levels = 5; // Create a ladder of 5 quote levels on each side
        double level_spacing = std::max(0.05, spread * 0.25); // Minimum spacing of 5 cents, or 25% of the optimal spread

        for (int i = 0; i < num_levels; ++i) {
            double current_bid = bid_price - (i * level_spacing);
            double current_ask = ask_price + (i * level_spacing);
            
            // Provide more liquidity deeper in the book to absorb shocks (1x, 1.5x, 2x...)
            double current_qty = m_lot_size * (1.0 + i * 0.5);

            m_fix_client->send_order(ticker, current_bid, current_qty, "BUY");
            m_fix_client->send_order(ticker, current_ask, current_qty, "SELL");
        }
    }

private:
    std::shared_ptr<MarketDataHandler> m_md_handler;
    std::shared_ptr<MMFixClient> m_fix_client;

    // Model parameters
    double m_initial_inventory;
    double m_lot_size;
    double m_gamma;         // Constant gamma risk parameter
    double m_k;             // Order book liquidity parameter
    double m_terminal_time; // T - t (time remaining)

    std::vector<std::string> m_tickers;

    std::atomic<bool> m_running;
    std::thread m_worker_thread;
    std::chrono::time_point<std::chrono::steady_clock> m_start_time;
};

}
