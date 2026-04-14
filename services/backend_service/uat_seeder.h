#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <print>
#include <random>
#include <string>
#include <vector>

#include <core/constants.h>
#include <database/database_client.h>
#include <questdb/ingress/line_sender.hpp>

namespace backend {

using namespace questdb::ingress::literals;

// Populates the databases with realistic-looking test data when the backend is
// started in UAT mode (uat = true in config).  All generated data is tracked by
// ID so it can be wiped cleanly on shutdown.
//
// Created accounts
//   username      password   role on UAT_Server
//   uat_admin     uat_pass   admin
//   uat_trader1   uat_pass   member
//   uat_trader2   uat_pass   member
class UatSeeder {
public:
    explicit UatSeeder(database::DatabaseClient& db) : m_db{db} {}

    // Returns true when seeding succeeds (or partially succeeds with warnings).
    bool seed() {
        std::println("[UAT] Seeding test data...");

        struct UatUser { std::string name; std::string password; };
        const std::vector<UatUser> uat_users = {
            {"uat_admin",   "uat_pass"},
            {"uat_trader1", "uat_pass"},
            {"uat_trader2", "uat_pass"},
        };

        for (const auto& u : uat_users) {
            auto result = m_db.create_user(u.name, u.password);
            if (!result.has_value()) {
                // User might already exist from a previous unclean shutdown.
                auto existing = m_db.get_user(u.name);
                if (!existing.has_value() || !existing.value().has_value()) {
                    std::println("[UAT] Error: cannot create or find user {}: {}",
                                 u.name, result.error());
                    return false;
                }
                m_uat_user_ids.push_back(existing.value()->user_id);
                std::println("[UAT] Reusing existing user {} (id={})",
                             u.name, existing.value()->user_id);
            } else {
                m_uat_user_ids.push_back(result.value().user_id);
                std::println("[UAT] Created user {} (id={})",
                             u.name, result.value().user_id);
            }
        }

        // m_uat_user_ids: [0]=uat_admin [1]=uat_trader1 [2]=uat_trader2
        const int admin_id   = m_uat_user_ids[0];
        const int trader1_id = m_uat_user_ids[1];
        const int trader2_id = m_uat_user_ids[2];

        const std::vector<std::string> symbols   = {"AAPL", "GOOGL", "TSLA"};
        const std::vector<int>         allowlist  = {trader1_id, trader2_id};

        auto srv_result = m_db.create_server(
            "UAT_Server", admin_id,
            "UAT Test Server — auto-generated for frontend testing",
            symbols, allowlist, 100000);

        int server_id{};
        if (!srv_result.has_value()) {
            // Server may already exist from a previous unclean shutdown — reuse it.
            auto existing = m_db.get_server("UAT_Server");
            if (!existing.has_value() || !existing.value().has_value()) {
                std::println("[UAT] Error: cannot create or find server: {}", srv_result.error());
                return false;
            }
            server_id = existing.value()->server_id;
            std::println("[UAT] Reusing existing server UAT_Server (id={})", server_id);
        } else {
            server_id = srv_result.value();
            std::println("[UAT] Created server UAT_Server (id={})", server_id);
        }
        m_uat_server_ids.push_back(server_id);

        // Initial balances: USD cash + stock holdings per user.
        // Balance in DB should be multiplied by square of decimal_to_int_multiplier
        const std::int64_t balance_multiplier_squared = static_cast<std::int64_t>(
            core::constants::decimal_to_int_multiplier * core::constants::decimal_to_int_multiplier);
        
        struct BalanceSeed { int user_id; std::string symbol; std::int64_t balance; };
        const std::vector<BalanceSeed> balances = {
            {admin_id,   "USD",   100000}, {admin_id,   "AAPL",  50},
            {admin_id,   "GOOGL", 30},     {admin_id,   "TSLA",  20},
            {trader1_id, "USD",   85000},  {trader1_id, "AAPL",  100},
            {trader1_id, "GOOGL", 0},      {trader1_id, "TSLA",  40},
            {trader2_id, "USD",   90000},  {trader2_id, "AAPL",  0},
            {trader2_id, "GOOGL", 60},     {trader2_id, "TSLA",  15},
        };

        for (const auto& b : balances) {
            const std::int64_t scaled_balance = b.balance * balance_multiplier_squared;
            auto res = m_db.insert_balance(b.user_id, server_id, b.symbol, scaled_balance);
            if (!res.has_value())
                std::println("[UAT] Warning: balance insert failed (user={} sym={}): {}",
                             b.user_id, b.symbol, res.error());
        }

        seed_trades(symbols);

        std::println("[UAT] Seeding complete.");
        std::println("[UAT]   Users:  uat_admin / uat_trader1 / uat_trader2  (password: uat_pass)");
        std::println("[UAT]   Server: UAT_Server  (symbols: AAPL, GOOGL, TSLA)");
        return true;
    }

    // Removes all UAT data created by seed().  Safe to call more than once.
    void cleanup() {
        std::println("[UAT] Cleaning up test data...");

        auto pg_result = m_db.delete_uat_data(m_uat_user_ids, m_uat_server_ids);
        if (!pg_result.has_value())
            std::println("[UAT] Warning during PostgreSQL cleanup: {}", pg_result.error());

        // QuestDB does not support row-level deletes; truncate both timeseries
        // tables since UAT mode is intended to run in an isolated environment.
        auto t_result = m_db.truncate_trades();
        if (!t_result.has_value())
            std::println("[UAT] Warning: could not truncate trades: {}", t_result.error());

        auto o_result = m_db.truncate_orders();
        if (!o_result.has_value())
            std::println("[UAT] Warning: could not truncate orders: {}", o_result.error());

        m_uat_user_ids.clear();
        m_uat_server_ids.clear();
        std::println("[UAT] Cleanup complete.");
    }

private:
    // Insert synthetic trades directly via QuestDB ILP so we can specify
    // explicit timestamps spread over the last two hours.
    // Prices follow a random walk: each tick is a small normally-distributed
    // step from the previous price, producing realistic-looking chart data.
    void seed_trades(const std::vector<std::string>& symbols) {
        using namespace std::chrono;

        struct SymbolConfig {
            std::string sym;
            double base_price;  // in cents
            double volatility;  // std-dev of per-step price change (cents)
            int    min_price;   // floor to keep prices sensible
        };
        const std::vector<SymbolConfig> sym_cfg = {
            {"AAPL",  17000.0, 30.0,  15000},  // ~$170, ±$0.30/step
            {"GOOGL", 280000.0, 400.0, 250000}, // ~$2800, ±$4/step
            {"TSLA",  22000.0, 80.0,  18000},  // ~$220, ±$0.80/step
        };

        try {
            auto sender = questdb::ingress::line_sender::from_conf("tcp::addr=localhost:9009");
            auto buffer = sender.new_buffer();

            std::mt19937 rng{std::random_device{}()};

            const auto now_us = duration_cast<microseconds>(
                system_clock::now().time_since_epoch()).count();
            constexpr long long two_hours_us = 2LL * 60 * 60 * 1'000'000;
            constexpr int n_trades = 200; // trades per symbol
            // Space trades evenly; a small jitter keeps timestamps non-identical.
            const long long step_us = two_hours_us / n_trades;
            std::uniform_int_distribution<long long> jitter_dist(0, step_us / 4);

            int trade_id = 10000;
            int total_inserted = 0;

            for (const auto& sc : sym_cfg) {
                if (std::find(symbols.begin(), symbols.end(), sc.sym) == symbols.end())
                    continue;

                std::normal_distribution<double> step_dist(0.0, sc.volatility);
                std::uniform_int_distribution<int> qty_dist(1, 200);

                double price = sc.base_price;
                long long ts_us = now_us - two_hours_us;

                for (int i = 0; i < n_trades; ++i) {
                    // Random-walk price step; clamp to floor.
                    price = std::max(static_cast<double>(sc.min_price),
                                     price + step_dist(rng));
                    ts_us += step_us + jitter_dist(rng);

                    buffer.table("trades"_tn)
                        .symbol("symbol"_cn,        sc.sym)
                        .column("price"_cn,          static_cast<int64_t>(std::llround(price)))
                        .column("quantity"_cn,       static_cast<int64_t>(qty_dist(rng)))
                        .column("trade_id"_cn,       static_cast<int64_t>(trade_id++))
                        .column("taker_id"_cn,       static_cast<int64_t>(1))
                        .column("maker_id"_cn,       static_cast<int64_t>(2))
                        .column("taker_order_id"_cn, static_cast<int64_t>(i + 1))
                        .column("maker_order_id"_cn, static_cast<int64_t>(i + 1000))
                        .column("is_taker_buyer"_cn, i % 2 == 0)
                        .at(questdb::ingress::timestamp_micros{ts_us});

                    ++total_inserted;
                }
            }

            sender.flush(buffer);
            std::println("[UAT] Inserted {} trades into QuestDB ({} per symbol)",
                         total_inserted, n_trades);
        } catch (const std::exception& e) {
            std::println("[UAT] Warning: trade seeding failed: {}", e.what());
        }
    }

    database::DatabaseClient& m_db;
    std::vector<int> m_uat_user_ids;
    std::vector<int> m_uat_server_ids;
};

} // namespace backend
