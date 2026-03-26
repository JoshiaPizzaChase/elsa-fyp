#pragma once

#include <atomic>
#include <chrono>
#include <core/containers.h>
#include <core/orders.h>
#include <core/service.h>
#include <core/thread_safe_queue.h>
#include <core/trade.h>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <pqxx/pqxx>
#include <questdb/ingress/line_sender.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <cstdlib>
#include "config.h"

// These macros are only used for those server-specific services, e.g. OMS
#define ORDERS_TABLE (std::string("orders_") + SERVER_NAME)
#define TRADES_TABLE (std::string("trades_") + SERVER_NAME)

inline questdb::ingress::table_name_view TRADES_TABLE_NAME_VIEW{TRADES_TABLE.c_str(), TRADES_TABLE.length()};
inline questdb::ingress::table_name_view ORDERS_TABLE_NAME_VIEW{TRADES_TABLE.c_str(), ORDERS_TABLE.length()};

namespace database {

using namespace std::literals::string_view_literals;
using namespace questdb::ingress::literals;
using namespace std::chrono_literals;
using namespace core;

// Enum to string helpers since questdb does not support enum types natively.
inline std::string_view to_string(Side s) {
    switch (s) {
    case Side::bid:
        return "BID";
    case Side::ask:
        return "ASK";
    default:
        return "UNKNOWN";
    }
}

inline std::string_view to_string(OrderType t) {
    switch (t) {
    case OrderType::limit:
        return "LIMIT";
    case OrderType::market:
        return "MARKET";
    default:
        return "UNKNOWN";
    }
}

inline std::string_view to_string(TimeInForce tif) {
    switch (tif) {
    case TimeInForce::day:
        return "DAY";
    case TimeInForce::gtc:
        return "GTC";
    default:
        return "UNKNOWN";
    }
}

inline std::string_view to_string(ExecTypeOrOrderStatus s) {
    switch (s) {
    case OrderStatus::status_new:
        return "NEW";
    case OrderStatus::status_partially_filled:
        return "PARTIALLY_FILLED";
    case OrderStatus::status_filled:
        return "FILLED";
    case OrderStatus::status_canceled:
        return "CANCELED";
    case OrderStatus::status_pending_cancel:
        return "PENDING_CANCEL";
    case OrderStatus::status_rejected:
        return "REJECTED";
    default:
        return "UNKNOWN";
    }
}

// Write tasks for async writer.
// Mainly an abstraction layer so that std::variant can be used cleanly.
struct OrderInsertionTask {
    int internal_order_id;
    core::NewOrderSingleContainer new_order_request;
};

struct CancelInsertionTask {
    core::CancelOrderRequestContainer cancel_order_request;
};

struct ExecutionInsertionTask {
    core::ExecutionReportContainer execution_report;
};

struct TradeInsertionTask {
    Trade trade;
};

using WriteTask = std::variant<OrderInsertionTask, CancelInsertionTask, ExecutionInsertionTask,
                               TradeInsertionTask>;

// Async writer for QuestDB ILP protocol.
class AsyncWriter {
  public:
    explicit AsyncWriter(ThreadSafeQueue<WriteTask>& write_queue, int flush_threshold = 64,
                         std::chrono::milliseconds flush_interval = 100ms)
        : m_write_queue{write_queue}, m_flush_threshold{flush_threshold},
          m_flush_interval{flush_interval},
          m_sender{questdb::ingress::line_sender::from_conf("tcp::addr=localhost:9009")},
          m_buffer{m_sender.new_buffer()}, m_stop{false},
          m_writer_thread{[this] { writer_loop(); }} {
    }

    ~AsyncWriter() {
        m_stop.store(true);
        if (m_writer_thread.joinable()) {
            m_writer_thread.join();
        }

        while (auto task = m_write_queue.dequeue()) {
            std::visit([this](const auto& t) { append(t); }, *task);
        }

        try {
            if (m_buffer.size() > 0) {
                m_sender.flush(m_buffer);
            }
        } catch (...) {
            // TODO: logging
        }
    }

  private:
    void writer_loop() {
        auto last_flush = std::chrono::steady_clock::now();
        // TODO: switch to wait and dequeue
        while (!m_stop.load()) {
            while (auto task = m_write_queue.dequeue()) {
                std::visit([this](const auto& t) { append(t); }, *task);
            }

            const auto now{std::chrono::steady_clock::now()};
            if ((m_buffer.row_count() > 0 && (now - last_flush > m_flush_interval)) ||
                (m_buffer.row_count() >= static_cast<size_t>(m_flush_threshold))) {
                m_sender.flush(m_buffer);
                last_flush = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10ms});
        }
    }

    void append(const OrderInsertionTask& order) {
        const auto new_order_request = order.new_order_request;

        try {
            const auto orders_table = ORDERS_TABLE_NAME_VIEW;
            const auto order_id = "order_id"_cn;
            const auto cl_order_id = "cl_order_id"_cn;
            const auto sender_comp_id = "sender_comp_id"_cn;
            const auto symbol = "symbol"_cn;
            const auto side = "side"_cn;
            const auto order_qty = "order_qty"_cn;
            const auto filled_qty = "filled_qty"_cn;
            const auto ord_type = "ord_type"_cn;
            const auto price = "price"_cn;
            const auto time_in_force = "time_in_force"_cn;
            const auto order_status = "order_status"_cn;

            m_buffer.table(orders_table)
                .symbol(symbol, new_order_request.symbol)
                .symbol(side, to_string(new_order_request.side))
                .symbol(ord_type, to_string(new_order_request.ord_type))
                .symbol(time_in_force, to_string(new_order_request.time_in_force))
                .symbol(order_status, std::string_view{"NEW"})
                .column(sender_comp_id, new_order_request.sender_comp_id)
                .column(order_id, static_cast<std::int64_t>(order.internal_order_id))
                .column(cl_order_id,
                        static_cast<std::int64_t>(std::stoll(new_order_request.cl_ord_id)))
                .column(order_qty, static_cast<std::int64_t>(new_order_request.order_qty))
                .column(filled_qty, static_cast<std::int64_t>(0))
                // price is optional for market orders; store 0 if not present
                .column(price, static_cast<std::int64_t>(
                                   new_order_request.price ? *new_order_request.price : 0))
                // initial order_status is NEW
                .at(questdb::ingress::timestamp_micros::now());

            return;

        } catch (const std::exception& e) {
            // TODO: logging

            // return std::unexpected{
            //     std::format("Error faced when inserting order: {}", e.what())};
        }
    }

    // append methods for adding to line sender buffer
    void append(const CancelInsertionTask& cancel) {
        const auto cancel_order_request{cancel.cancel_order_request};

        try {
            const auto orders_table = ORDERS_TABLE_NAME_VIEW;
            const auto order_id = "order_id"_cn;
            const auto cl_order_id = "cl_order_id"_cn;
            const auto sender_comp_id = "sender_comp_id"_cn;
            const auto symbol = "symbol"_cn;
            const auto side = "side"_cn;
            const auto order_qty = "order_qty"_cn;
            const auto filled_qty = "filled_qty"_cn;
            const auto order_status = "order_status"_cn;

            // Represent cancel as an order row with status CANCELED and filled_qty = 0.
            m_buffer.table(orders_table)
                .symbol(symbol, cancel_order_request.symbol)
                .symbol(side, to_string(cancel_order_request.side))
                .symbol(order_status, std::string_view{"CANCELED"})
                .column(sender_comp_id, cancel_order_request.sender_comp_id)
                .column(order_id,
                        static_cast<int64_t>(std::stoll(cancel_order_request.order_id.value())))
                .column(cl_order_id,
                        static_cast<int64_t>(std::stoll(cancel_order_request.cl_ord_id)))
                .column(order_qty, static_cast<std::int64_t>(cancel_order_request.order_qty))
                .column(filled_qty, static_cast<std::int64_t>(0))
                .at(questdb::ingress::timestamp_micros::now());

            return;
        } catch (const std::exception& e) {
            // TODO: logger

            // return std::unexpected{
            //     std::format("Error faced when inserting cancel: {}", e.what())};
        }
    }

    void append(const ExecutionInsertionTask& exec_report) {
        const auto execution_report = exec_report.execution_report;
        try {
            const auto orders_table = ORDERS_TABLE_NAME_VIEW;
            const auto order_id = "order_id"_cn;
            const auto cl_order_id = "cl_order_id"_cn;
            const auto sender_comp_id = "sender_comp_id"_cn;
            const auto symbol = "symbol"_cn;
            const auto side = "side"_cn;
            const auto order_qty = "order_qty"_cn;
            const auto filled_qty = "filled_qty"_cn;
            const auto price = "price"_cn;
            const auto time_in_force = "time_in_force"_cn;
            const auto order_status = "order_status"_cn;

            // Map execution report to orders table row; use cum_qty as filled_qty and
            // ord_status from the report.
            m_buffer.table(orders_table)
                .symbol(symbol, execution_report.symbol)
                .symbol(side, to_string(execution_report.side))
                .symbol(order_status, to_string(execution_report.ord_status))
                .symbol(time_in_force, execution_report.time_in_force
                                           ? to_string(*execution_report.time_in_force)
                                           : std::string_view{"UNKNOWN"})
                .column(sender_comp_id, execution_report.sender_comp_id)
                .column(order_id, static_cast<std::int64_t>(std::stoll(execution_report.order_id)))
                .column(cl_order_id,
                        static_cast<std::int64_t>(std::stoll(execution_report.cl_order_id)))
                .column(order_qty, static_cast<std::int64_t>(execution_report.leaves_qty +
                                                             execution_report.cum_qty))
                .column(filled_qty, static_cast<std::int64_t>(execution_report.cum_qty))
                .column(price, static_cast<std::int64_t>(
                                   execution_report.price ? *execution_report.price : 0))
                .at(questdb::ingress::timestamp_micros::now());

            return;
        } catch (const std::exception& e) {
            // TODO: logger

            // return std::unexpected{std::format(
            //     "Error faced when inserting execution: {}", e.what())};
        }
    }

    void append(const TradeInsertionTask& trade_task) {
        const auto trade{trade_task.trade};
        
        try {
            const auto trades_table = TRADES_TABLE_NAME_VIEW;
            const auto symbol = "symbol"_cn;
            const auto price = "price"_cn;
            const auto quantity_cn = "quantity"_cn;
            const auto trade_id_cn = "trade_id"_cn;
            const auto taker_id_cn = "taker_id"_cn;
            const auto maker_id_cn = "maker_id"_cn;
            const auto taker_order_id_cn = "taker_order_id"_cn;
            const auto maker_order_id_cn = "maker_order_id"_cn;
            const auto is_taker_buyer_cn = "is_taker_buyer"_cn;

            m_buffer
                .table(trades_table)
                // symbols
                .symbol(symbol, trade.ticker)
                // columns
                .column(price, static_cast<std::int64_t>(trade.price))
                .column(quantity_cn, static_cast<std::int64_t>(trade.quantity))
                .column(trade_id_cn, static_cast<std::int64_t>(trade.trade_id))
                .column(taker_id_cn, static_cast<std::int64_t>(trade.taker_id))
                .column(maker_id_cn, static_cast<std::int64_t>(trade.maker_id))
                .column(taker_order_id_cn, static_cast<std::int64_t>(trade.taker_order_id))
                .column(maker_order_id_cn, static_cast<std::int64_t>(trade.maker_order_id))
                .column(is_taker_buyer_cn, trade.is_taker_buyer)
                .at(questdb::ingress::timestamp_micros::now());

            return;
        } catch (const std::exception& e) {
            // TODO: logger

            // return std::unexpected{
            //     std::format("Error faced when inserting trade: {}", e.what())};
        }
    }

    ThreadSafeQueue<WriteTask>& m_write_queue;

    int m_flush_threshold;
    std::chrono::milliseconds m_flush_interval;

    questdb::ingress::line_sender m_sender;
    questdb::ingress::line_sender_buffer m_buffer;
    std::atomic<bool> m_stop;

    std::thread m_writer_thread;
};

class DatabaseClient {
  public:
    struct ServiceInsertRow;

    /*
     * @param ensure_init Whether to ensure the timeseries connection and async writer on
     * construction. It seems like too many concurrent connections cause a seg fault, especially for
     * QuestDB. See the functions for justification.
     *
     * @param async_threshold The number of write tasks to accumulate before flushing.
     * @param async_flush_interval The interval at which to flush write tasks.
     */
    DatabaseClient(bool ensure_init = false, int async_threshold = 64,
                   std::chrono::milliseconds async_flush_interval = 100ms)
        : m_flush_interval(async_flush_interval), m_flush_threshold(async_threshold) {
        if (ensure_init) {
            ensure_timeseries_connection();
            ensure_async_writer();
        }
    }

    DatabaseClient& operator=(const DatabaseClient&) = delete;
    DatabaseClient& operator=(DatabaseClient&&) = delete;

    DatabaseClient(const DatabaseClient&) = delete;
    DatabaseClient(DatabaseClient&&) = delete;

    // Usually called by the OM at initialization, so this can be left synchronous.
    auto read_balance(int user_id, int server_id, std::string_view symbol)
        -> std::expected<int, std::string> {
        try {
            pqxx::work transaction{*m_core_db_sql_connection};

            int balance{transaction.query_value<int>("SELECT balance FROM balances WHERE user_id = "
                                                     "$1 AND symbol = $2 AND server_id = $3",
                                                     pqxx::params{user_id, symbol, server_id})};

            transaction.commit();

            return balance;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when getting balance: {}", e.what())};
        }
    }

    struct BalanceRow {
        std::string symbol;
        int balance;
    };

    auto read_balances(int user_id, int server_id) const
        -> std::expected<std::vector<BalanceRow>, std::string> {
        try {
            pqxx::work transaction{*m_core_db_sql_connection};
            std::vector<BalanceRow> balances;
            auto res = transaction.exec(
                "SELECT symbol, balance FROM balances WHERE user_id = $1 AND server_id = $2",
                pqxx::params{user_id, server_id});
            transaction.commit();

            balances.reserve(res.size());
            for (const auto& row : res) {
                balances.emplace_back(
                    BalanceRow{row["symbol"].as<std::string>(), row["balance"].as<int>()});
            }

            return balances;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when getting balance: {}", e.what())};
        }
    }

    // This is called periodically (e.g. hourly) by the OM so it should be performed asynchronously.
    auto update_balance(int user_id, int server_id, std::string_view symbol, int balance) const
        -> std::expected<void, std::string> {
        try {
            pqxx::work transaction{*m_core_db_sql_connection};

            transaction.exec("UPDATE balances SET balance = $4 WHERE user_id = $1 AND server_id = "
                             "$2 AND symbol = $3",
                             pqxx::params{user_id, server_id, symbol, balance});

            transaction.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when updating balance: {}", e.what())};
        }
    }

    struct UserRow {
        int user_id{};
        std::string username;
    };

    struct ServerRow {
        int server_id{};
        int admin_id{};
        std::string server_name;
        std::string admin_name;
        std::vector<std::string> active_tickers;
        std::string description;
        int initial_usd{100000};
    };

    struct UserServerRow {
        int server_id{};
        std::string server_name;
        std::string role;
        std::vector<std::string> active_tickers;
        std::string description;
        int initial_usd{100000};
        std::vector<BalanceRow> balances;
    };

    struct AccountDetailsRow {
        int server_id{};
        std::string server_name;
        std::string admin_name;
        std::string description;
        std::vector<std::string> active_tickers;
        std::string role;
        int initial_usd{100000};
        std::vector<BalanceRow> balances;
    };

    struct HistoricalTradeRow {
        int trade_id{};
        std::string symbol;
        int price{};
        int quantity{};
        long long ts_ms{};
    };

    // Verify credentials; returns the user row on success or nullopt on mismatch.
    auto authenticate_user(std::string_view username, std::string_view password)
        -> std::expected<std::optional<UserRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec(
                "SELECT user_id, username FROM users WHERE username = $1 AND password = $2",
                pqxx::params{username, password});
            txn.commit();
            if (res.empty())
                return std::nullopt;
            return UserRow{res[0]["user_id"].as<int>(), res[0]["username"].as<std::string>()};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error authenticating user: {}", e.what())};
        }
    }

    // Insert a new user; returns the newly created row.
    auto create_user(std::string_view username, std::string_view password)
        -> std::expected<UserRow, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("INSERT INTO users (username, password) VALUES ($1, $2) RETURNING "
                                "user_id, username",
                                pqxx::params{username, password});
            txn.commit();
            if (res.empty())
                return std::unexpected{std::string{"Failed to create user"}};
            return UserRow{res[0]["user_id"].as<int>(), res[0]["username"].as<std::string>()};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error creating user: {}", e.what())};
        }
    }

    // Lookup a user by username; returns nullopt when not found.
    auto get_user(std::string_view username) -> std::expected<std::optional<UserRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT user_id, username FROM users WHERE username = $1",
                                pqxx::params{username});
            txn.commit();
            if (res.empty())
                return std::nullopt;
            return UserRow{res[0]["user_id"].as<int>(), res[0]["username"].as<std::string>()};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting user: {}", e.what())};
        }
    }

    auto get_active_servers() -> std::expected<std::vector<ServerRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            const bool has_initial_usd = servers_has_initial_usd_column(txn);
            auto res = has_initial_usd
                           ? txn.exec("SELECT s.server_id, s.admin_id, s.server_name, "
                                      "u.username AS admin_name, "
                                      "s.active_tickers, s.description, s.initial_usd "
                                      "FROM servers s JOIN users u ON s.admin_id = u.user_id")
                           : txn.exec("SELECT s.server_id, s.admin_id, s.server_name, "
                                      "u.username AS admin_name, "
                                      "s.active_tickers, s.description "
                                      "FROM servers s JOIN users u ON s.admin_id = u.user_id");
            txn.commit();
            std::vector<ServerRow> result;
            result.reserve(res.size());
            for (const auto& row : res) {
                ServerRow srv;
                srv.server_id = row["server_id"].as<int>();
                srv.admin_id = row["admin_id"].as<int>();
                srv.server_name = row["server_name"].as<std::string>();
                srv.admin_name = row["admin_name"].as<std::string>();
                srv.description = row["description"].as<std::string>("");
                srv.active_tickers = parse_pg_array(row["active_tickers"].as<std::string>("{}"));
                srv.initial_usd = has_initial_usd ? row["initial_usd"].as<int>(100000) : 100000;
                result.push_back(std::move(srv));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting active servers: {}", e.what())};
        }
    }

    auto get_server(const std::string_view& server_name)
        -> std::expected<std::optional<ServerRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            const bool has_initial_usd = servers_has_initial_usd_column(txn);
            auto res = has_initial_usd
                           ? txn.exec("SELECT s.server_id, s.admin_id, s.server_name, "
                                      "u.username AS admin_name, s.active_tickers, "
                                      "s.description, s.initial_usd "
                                      "FROM servers s JOIN users u ON s.admin_id = u.user_id "
                                      "WHERE s.server_name = $1",
                                      pqxx::params{server_name})
                           : txn.exec("SELECT s.server_id, s.admin_id, s.server_name, "
                                      "u.username AS admin_name, s.active_tickers, "
                                      "s.description "
                                      "FROM servers s JOIN users u ON s.admin_id = u.user_id "
                                      "WHERE s.server_name = $1",
                                      pqxx::params{server_name});
            txn.commit();
            if (res.empty())
                return std::nullopt;
            const auto& row = res[0];
            ServerRow srv;
            srv.server_id = row["server_id"].as<int>();
            srv.admin_id = row["admin_id"].as<int>();
            srv.server_name = row["server_name"].as<std::string>();
            srv.admin_name = row["admin_name"].as<std::string>();
            srv.description = row["description"].as<std::string>("");
            srv.active_tickers = parse_pg_array(row["active_tickers"].as<std::string>("{}"));
            srv.initial_usd = has_initial_usd ? row["initial_usd"].as<int>(100000) : 100000;
            return srv;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting server: {}", e.what())};
        }
    }

    // Active tickers for a server; returns an empty vector when not found.
    auto get_server_active_symbols(std::string_view server_name)
        -> std::expected<std::vector<std::string>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT active_tickers FROM servers WHERE server_name = $1",
                                pqxx::params{server_name});
            txn.commit();
            if (res.empty())
                return std::vector<std::string>{};
            return parse_pg_array(res[0]["active_tickers"].as<std::string>("{}"));
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting active symbols: {}", e.what())};
        }
    }

    // All servers a user is associated with (as admin or member), each entry
    // carrying the user's full balance list.
    auto get_user_servers(std::string_view username)
        -> std::expected<std::vector<UserServerRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};

            auto user_res =
                txn.exec("SELECT user_id FROM users WHERE username = $1", pqxx::params{username});
            if (user_res.empty())
                return std::vector<UserServerRow>{};
            const int user_id = user_res[0]["user_id"].as<int>();

            const bool has_initial_usd = servers_has_initial_usd_column(txn);
            auto srv_res = has_initial_usd
                               ? txn.exec("SELECT s.server_id, s.server_name, s.active_tickers, "
                                          "s.description, s.initial_usd, "
                                          "CASE WHEN s.admin_id = $1 THEN 'admin' ELSE 'member' "
                                          "END AS role "
                                          "FROM servers s "
                                          "WHERE s.admin_id = $1 "
                                          "OR EXISTS (SELECT 1 FROM allowlist a WHERE "
                                          "a.server_id = s.server_id AND "
                                          "a.user_id = $1)",
                                          pqxx::params{user_id})
                               : txn.exec("SELECT s.server_id, s.server_name, s.active_tickers, "
                                          "s.description, "
                                          "CASE WHEN s.admin_id = $1 THEN 'admin' ELSE 'member' "
                                          "END AS role "
                                          "FROM servers s "
                                          "WHERE s.admin_id = $1 "
                                          "OR EXISTS (SELECT 1 FROM allowlist a WHERE "
                                          "a.server_id = s.server_id AND "
                                          "a.user_id = $1)",
                                          pqxx::params{user_id});

            auto bal_res = txn.exec("SELECT symbol, balance FROM balances WHERE user_id = $1",
                                    pqxx::params{user_id});
            txn.commit();

            std::vector<BalanceRow> balances;
            balances.reserve(bal_res.size());
            for (const auto& row : bal_res)
                balances.push_back({row["symbol"].as<std::string>(), row["balance"].as<int>()});

            std::vector<UserServerRow> result;
            result.reserve(srv_res.size());
            for (const auto& row : srv_res) {
                UserServerRow usr;
                usr.server_id = row["server_id"].as<int>();
                usr.server_name = row["server_name"].as<std::string>();
                usr.role = row["role"].as<std::string>();
                usr.description = row["description"].as<std::string>("");
                usr.active_tickers = parse_pg_array(row["active_tickers"].as<std::string>("{}"));
                usr.initial_usd = has_initial_usd ? row["initial_usd"].as<int>(100000) : 100000;
                usr.balances = balances;
                result.push_back(std::move(usr));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting user servers: {}", e.what())};
        }
    }

    auto get_account_details(std::string_view username, std::string_view server_name)
        -> std::expected<std::optional<AccountDetailsRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};

            const bool has_initial_usd = servers_has_initial_usd_column(txn);
            auto res = has_initial_usd
                           ? txn.exec(
                                 "SELECT s.server_id, s.server_name, s.description, "
                                 "s.active_tickers, s.initial_usd, "
                                 "u_admin.username AS admin_name, u.user_id, "
                                 "CASE WHEN s.admin_id = u.user_id THEN 'admin' ELSE 'member' "
                                 "END AS role "
                                 "FROM users u "
                                 "JOIN servers s ON s.server_name = $2 "
                                 "JOIN users u_admin ON u_admin.user_id = s.admin_id "
                                 "WHERE u.username = $1 "
                                 "AND (s.admin_id = u.user_id "
                                 "     OR EXISTS (SELECT 1 FROM allowlist a "
                                 "                WHERE a.server_id = s.server_id AND "
                                 "a.user_id = u.user_id))",
                                 pqxx::params{username, server_name})
                           : txn.exec(
                                 "SELECT s.server_id, s.server_name, s.description, "
                                 "s.active_tickers, "
                                 "u_admin.username AS admin_name, u.user_id, "
                                 "CASE WHEN s.admin_id = u.user_id THEN 'admin' ELSE 'member' "
                                 "END AS role "
                                 "FROM users u "
                                 "JOIN servers s ON s.server_name = $2 "
                                 "JOIN users u_admin ON u_admin.user_id = s.admin_id "
                                 "WHERE u.username = $1 "
                                 "AND (s.admin_id = u.user_id "
                                 "     OR EXISTS (SELECT 1 FROM allowlist a "
                                 "                WHERE a.server_id = s.server_id AND "
                                 "a.user_id = u.user_id))",
                                 pqxx::params{username, server_name});
            if (res.empty())
                return std::nullopt;

            const auto& row = res[0];
            AccountDetailsRow details;
            const int server_id = row["server_id"].as<int>();
            const int user_id = row["user_id"].as<int>();
            details.server_id = server_id;
            details.server_name = row["server_name"].as<std::string>();
            details.admin_name = row["admin_name"].as<std::string>();
            details.description = row["description"].as<std::string>("");
            details.role = row["role"].as<std::string>();
            details.active_tickers = parse_pg_array(row["active_tickers"].as<std::string>("{}"));
            details.initial_usd = has_initial_usd ? row["initial_usd"].as<int>(100000) : 100000;

            // Build the PostgreSQL array literal for active tickers
            const std::string arr_lit = build_pg_array(details.active_tickers);
            auto bal_res =
                txn.exec("SELECT symbol, balance FROM balances "
                         "WHERE user_id = $1 AND (symbol = 'USD' OR symbol = ANY($2::varchar[]))",
                         pqxx::params{user_id, arr_lit});
            txn.commit();

            for (const auto& brow : bal_res)
                details.balances.push_back(
                    {brow["symbol"].as<std::string>(), brow["balance"].as<int>()});
            return details;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting account details: {}", e.what())};
        }
    }

    // Trades for a symbol from QuestDB after a given Unix timestamp (ms), ordered oldest-first.
    auto query_trades(const std::string_view& symbol, long long after_ts_ms)
        -> std::expected<std::vector<HistoricalTradeRow>, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            // QuestDB: CAST(ts AS LONG) returns microseconds since epoch.
            // Parameterised queries over QuestDB's PG wire are unreliable, so we
            // quote literals manually (safe: tickers are alphanumeric, after_ts_micros is numeric).
            const auto after_ts_micros = after_ts_ms * 1000LL;
            const std::string query =
                "SELECT trade_id, symbol, price, quantity, CAST(ts AS LONG) AS ts_micros "
                "FROM trades "
                "WHERE symbol = $1"
                " AND ts >= to_timestamp($2) "
                "ORDER BY ts ASC";
            auto res = txn.exec(query, pqxx::params{symbol, after_ts_micros});
            txn.commit();

            std::vector<HistoricalTradeRow> result;
            result.reserve(res.size());
            for (const auto& row : res) {
                HistoricalTradeRow trade;
                trade.trade_id = row["trade_id"].as<int>(0);
                trade.symbol = row["symbol"].as<std::string>("");
                trade.price = row["price"].as<int>(0);
                trade.quantity = row["quantity"].as<int>(0);
                trade.ts_ms = row["ts_micros"].as<long long>(0) / 1000;
                result.push_back(std::move(trade));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error querying trades: {}", e.what())};
        }
    }

    // Insert a new server and populate its allowlist in one transaction.
    // Returns the newly created server_id.
    auto create_server(std::string_view server_name, int admin_id, std::string_view description,
                       const std::vector<std::string>& symbols,
                       const std::vector<int>& allowlist_user_ids, int initial_usd = 100000)
        -> std::expected<int, std::string> {
        return create_server_with_services(server_name, admin_id, description, symbols,
                                           allowlist_user_ids, {}, initial_usd);
    }

    // Insert a new server, allowlist, and service endpoints in one transaction.
    // Returns the newly created server_id.
    auto create_server_with_services(std::string_view server_name, int admin_id,
                                     std::string_view description,
                                     const std::vector<std::string>& symbols,
                                     const std::vector<int>& allowlist_user_ids,
                                     const std::vector<ServiceInsertRow>& services,
                                     int initial_usd = 100000)
        -> std::expected<int, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            const std::string arr = build_pg_array(symbols);
            const bool has_initial_usd = servers_has_initial_usd_column(txn);
            auto res = has_initial_usd
                           ? txn.exec("INSERT INTO servers (server_name, admin_id, description, "
                                      "active_tickers, initial_usd) "
                                      "VALUES ($1, $2, $3, $4::varchar[], $5) RETURNING server_id",
                                      pqxx::params{server_name, admin_id, description, arr,
                                                   initial_usd})
                           : txn.exec("INSERT INTO servers (server_name, admin_id, description, "
                                      "active_tickers) "
                                      "VALUES ($1, $2, $3, $4::varchar[]) RETURNING server_id",
                                      pqxx::params{server_name, admin_id, description, arr});
            if (res.empty())
                return std::unexpected{std::string{"Failed to create server"}};
            const int server_id = res[0]["server_id"].as<int>();
            for (int uid : allowlist_user_ids) {
                txn.exec("INSERT INTO allowlist (server_id, user_id) VALUES ($1, $2)",
                         pqxx::params{server_id, uid});
            }
            for (const auto& service : services) {
                txn.exec("INSERT INTO services (machine_id, server_id, service_type, port) "
                         "VALUES ($1, $2, $3, $4)",
                         pqxx::params{service.machine_id, server_id,
                                      service_enum_to_str(service.service_type), service.port});
            }
            txn.commit();
            return server_id;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error creating server: {}", e.what())};
        }
    }

    // Update an existing server's metadata and replace its allowlist.
    // Returns false when the server is not found or caller_id is not the admin.
    auto configure_server(std::string_view server_name, int caller_id, std::string_view description,
                          const std::vector<std::string>& symbols,
                          const std::vector<int>& allowlist_user_ids)
        -> std::expected<bool, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto srv_res =
                txn.exec("SELECT server_id, admin_id FROM servers WHERE server_name = $1",
                         pqxx::params{server_name});
            if (srv_res.empty())
                return false;
            if (srv_res[0]["admin_id"].as<int>() != caller_id)
                return false;
            const int server_id = srv_res[0]["server_id"].as<int>();

            const std::string arr = build_pg_array(symbols);
            txn.exec("UPDATE servers SET description = $1, active_tickers = $2::varchar[] "
                     "WHERE server_id = $3",
                     pqxx::params{description, arr, server_id});

            txn.exec("DELETE FROM allowlist WHERE server_id = $1", pqxx::params{server_id});
            for (int uid : allowlist_user_ids) {
                txn.exec("INSERT INTO allowlist (server_id, user_id) VALUES ($1, $2)",
                         pqxx::params{server_id, uid});
            }
            txn.commit();
            return true;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error configuring server: {}", e.what())};
        }
    }

    auto delete_server(std::string_view server_name) -> std::expected<bool, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto srv_res =
                txn.exec("SELECT server_id FROM servers WHERE server_name = $1",
                         pqxx::params{server_name});
            if (srv_res.empty())
                return false;
            const int server_id = srv_res[0]["server_id"].as<int>();

            txn.exec("DELETE FROM allowlist WHERE server_id = $1", pqxx::params{server_id});
            txn.exec("DELETE FROM balances WHERE server_id = $1", pqxx::params{server_id});
            txn.exec("DELETE FROM services WHERE server_id = $1", pqxx::params{server_id});
            txn.exec("DELETE FROM servers WHERE server_id = $1", pqxx::params{server_id});
            txn.commit();
            return true;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error deleting server: {}", e.what())};
        }
    }

    // Resolve a list of usernames to user_ids; returns an error naming the first
    // unknown username.
    auto resolve_user_ids(const std::vector<std::string>& usernames)
        -> std::expected<std::vector<int>, std::string> {
        std::vector<int> ids;
        ids.reserve(usernames.size());
        for (const auto& uname : usernames) {
            auto res = get_user(uname);
            if (!res.has_value())
                return std::unexpected{res.error()};
            if (!res.value().has_value())
                return std::unexpected{"User not found: " + uname};
            ids.push_back(res.value()->user_id);
        }
        return ids;
    }

    // -----------------------------------------------------------------------
    // Insert-based functions for questdb should also be asynchronous.
    auto insert_order(int internal_order_id, const core::NewOrderSingleContainer& new_order_request)
        -> std::expected<void, std::string> {
        ensure_async_writer();
        m_write_queue.enqueue(OrderInsertionTask{internal_order_id, new_order_request});
        return {};
    }

    auto insert_cancel(const core::CancelOrderRequestContainer& cancel_order_request)
        -> std::expected<void, std::string> {
        assert(cancel_order_request.order_id.has_value());
        ensure_async_writer();
        m_write_queue.enqueue(CancelInsertionTask{cancel_order_request});
        return {};
    }

    auto insert_execution(const core::ExecutionReportContainer& execution_report)
        -> std::expected<void, std::string> {
        ensure_async_writer();
        m_write_queue.enqueue(ExecutionInsertionTask{execution_report});
        return {};
    }

    auto insert_trade(const Trade& trade) -> std::expected<void, std::string> {
        ensure_async_writer();
        m_write_queue.enqueue(TradeInsertionTask{trade});
        return {};
    }

    // TODO: this is public but maybe i should put this elsewhere
    struct OrderRow {
        int order_id{};
        int cl_order_id{};
        std::string sender_comp_id;
        std::string symbol;
        std::string side;
        int order_qty{};
        int filled_qty{};
        std::string ord_type;
        int price{};
        std::string time_in_force;
        std::string order_status;
    };

    struct TradeRow {
        int price{};
        int quantity{};
        std::string symbol;
        int trade_id{};
        int taker_id{};
        int maker_id{};
        int taker_order_id{};
        int maker_order_id{};
        bool is_taker_buyer{};
    };

    struct MachineRow {
        int machine_id{};
        std::string machine_name;
        std::string ip;
    };

    auto query_machines() const -> std::expected<std::vector<MachineRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT machine_id, machine_name, ip FROM machines;");
            txn.commit();
            std::vector<MachineRow> result;
            result.reserve(res.size());
            for (const auto& row : res) {
                MachineRow machine;
                machine.machine_id = row["machine_id"].as<int>();
                machine.ip = row["ip"].as<std::string>();
                machine.machine_name = row["machine_name"].as<std::string>();
                result.push_back(std::move(machine));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting active servers: {}", e.what())};
        }
    }

    struct ServiceRow {
        Service service_type{};
        std::string ip;
        int port{};
    };

    struct ServiceEndpoint {
        std::string ip;
        int port{};
    };

    struct ServiceInsertRow {
        int machine_id{};
        Service service_type{};
        int port{};
    };


    auto query_services() const -> std::expected<std::vector<ServiceRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT "
                                "s.service_type,"
                                "m.ip,"
                                "s.port::VARCHAR as port "
                                "FROM services s "
                                "INNER JOIN servers sv ON s.server_id = sv.server_id "
                                "INNER JOIN machines m ON s.machine_id = m.machine_id "
                                "WHERE sv.server_name = $1;");
            txn.commit();
            std::vector<ServiceRow> result;
            result.reserve(res.size());
            for (const auto& row : res) {
                ServiceRow service;
                service.service_type =
                    service_str_to_enum(row["service_type"].as<std::string>()).value();
                service.ip = row["ip"].as<std::string>();
                service.port = row["port"].as<int>();
                result.push_back(std::move(service));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting active servers: {}", e.what())};
        }
    }

    auto query_services(const std::string_view machine_name) const -> std::expected<std::vector<ServiceRow>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT "
                                "s.service_type,"
                                "m.ip,"
                                "s.port::VARCHAR as port "
                                "FROM services s "
                                "INNER JOIN servers sv ON s.server_id = sv.server_id "
                                "INNER JOIN machines m ON s.machine_id = m.machine_id "
                                "WHERE m.machine_name = $1;",
                                pqxx::params{machine_name});
            txn.commit();
            std::vector<ServiceRow> result;
            result.reserve(res.size());
            for (const auto& row : res) {
                ServiceRow service;
                service.service_type =
                    service_str_to_enum(row["service_type"].as<std::string>()).value();
                service.ip = row["ip"].as<std::string>();
                service.port = row["port"].as<int>();
                result.push_back(std::move(service));
            }
            return result;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error getting services of machine {}: {}", machine_name, e.what())};
        }
    }

    auto get_service_endpoint(const std::string_view server_name, Service service_type) const
        -> std::expected<std::optional<ServiceEndpoint>, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            auto res = txn.exec("SELECT m.ip, s.port::VARCHAR AS port "
                                "FROM services s "
                                "INNER JOIN servers sv ON s.server_id = sv.server_id "
                                "INNER JOIN machines m ON s.machine_id = m.machine_id "
                                "WHERE sv.server_name = $1 AND s.service_type = $2 "
                                "LIMIT 1;",
                                pqxx::params{server_name, service_enum_to_str(service_type)});
            txn.commit();
            if (res.empty())
                return std::nullopt;
            ServiceEndpoint endpoint;
            endpoint.ip = res[0]["ip"].as<std::string>();
            endpoint.port = res[0]["port"].as<int>();
            return endpoint;
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error getting service endpoint for {}: {}", server_name, e.what())};
        }
    }

    auto insert_service(int machine_id, int server_id, Service service_type, int port) const
        -> std::expected<void, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            txn.exec("INSERT INTO services (machine_id, server_id, service_type, port) "
                     "VALUES ($1, $2, $3, $4)",
                     pqxx::params{machine_id, server_id, service_enum_to_str(service_type), port});
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error inserting service: {}", e.what())};
        }
    }

    // Assuming these are not in hot path, so synchronous is fine.
    auto query_orders() -> std::expected<std::vector<OrderRow>, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            pqxx::result res =
                txn.exec("SELECT order_id, cl_order_id, sender_comp_id, symbol, side, "
                         "order_qty, filled_qty, ord_type, price, time_in_force, order_status "
                         "FROM orders");
            txn.commit();

            std::vector<OrderRow> rows;
            rows.reserve(res.size());
            for (const auto& r : res) {
                OrderRow row;
                row.order_id = r["order_id"].as<int>(0);
                row.cl_order_id = r["cl_order_id"].as<int>(0);
                row.sender_comp_id = r["sender_comp_id"].as<std::string>("");
                row.symbol = r["symbol"].as<std::string>("");
                row.side = r["side"].as<std::string>("");
                row.order_qty = r["order_qty"].as<int>(0);
                row.filled_qty = r["filled_qty"].as<int>(0);
                row.ord_type = r["ord_type"].as<std::string>("");
                row.price = r["price"].as<int>(0);
                row.time_in_force = r["time_in_force"].as<std::string>("");
                row.order_status = r["order_status"].as<std::string>("");
                rows.push_back(std::move(row));
            }
            return rows;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error querying orders: {}", e.what())};
        }
    }

    auto query_trades() -> std::expected<std::vector<TradeRow>, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            pqxx::result res =
                txn.exec("SELECT price, quantity, symbol, trade_id, taker_id, maker_id, "
                         "taker_order_id, maker_order_id, is_taker_buyer "
                         "FROM trades");
            txn.commit();

            std::vector<TradeRow> rows;
            rows.reserve(res.size());
            for (const auto& r : res) {
                TradeRow row;
                row.price = r["price"].as<int>(0);
                row.quantity = r["quantity"].as<int>(0);
                row.symbol = r["symbol"].as<std::string>("");
                row.trade_id = r["trade_id"].as<int>(0);
                row.taker_id = r["taker_id"].as<int>(0);
                row.maker_id = r["maker_id"].as<int>(0);
                row.taker_order_id = r["taker_order_id"].as<int>(0);
                row.maker_order_id = r["maker_order_id"].as<int>(0);
                row.is_taker_buyer = r["is_taker_buyer"].as<bool>(false);
                rows.push_back(std::move(row));
            }
            return rows;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error querying trades: {}", e.what())};
        }
    }

    auto insert_balance(int user_id, int server_id, std::string_view symbol, int balance)
        -> std::expected<void, std::string> {
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            txn.exec("INSERT INTO balances (user_id, server_id, symbol, balance) "
                     "VALUES ($1, $2, $3, $4)",
                     pqxx::params{user_id, server_id, symbol, balance});
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error inserting balance: {}", e.what())};
        }
    }

    // Remove UAT-generated rows from PostgreSQL.
    // Deletes in dependency order: allowlist -> balances -> servers -> users.
    auto delete_uat_data(const std::vector<int>& user_ids, const std::vector<int>& server_ids)
        -> std::expected<void, std::string> {
        if (user_ids.empty() && server_ids.empty())
            return {};
        try {
            pqxx::work txn{*m_core_db_sql_connection};
            for (int sid : server_ids)
                txn.exec("DELETE FROM allowlist WHERE server_id = $1", pqxx::params{sid});
            for (int uid : user_ids)
                txn.exec("DELETE FROM balances WHERE user_id = $1", pqxx::params{uid});
            for (int sid : server_ids)
                txn.exec("DELETE FROM servers WHERE server_id = $1", pqxx::params{sid});
            for (int uid : user_ids)
                txn.exec("DELETE FROM users WHERE user_id = $1", pqxx::params{uid});
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error deleting UAT data: {}", e.what())};
        }
    }

    auto create_quest_tables(const std::string_view& server_name)
        -> std::expected<void, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            auto trades_create_sql = std::format("CREATE TABLE IF NOT EXISTS trades_{} "
                        "(ts TIMESTAMP,"
                        "price INT,"
                        "quantity INT,"
                        "symbol SYMBOL,"
                        "trade_id INT,"
                        "taker_id INT,"
                        "maker_id INT,"
                        "taker_order_id INT,"
                        "maker_order_id INT,"
                        "is_taker_buyer BOOLEAN ) TIMESTAMP(ts) PARTITION BY DAY "
                        "DEDUP UPSERT KEYS(ts, trade_id);", server_name);
            txn.exec(trades_create_sql);
            auto orders_create_sql = std::format("CREATE TABLE IF NOT EXISTS orders_{} "
                        "(ts TIMESTAMP,"
                        "order_id INT,"
                        "cl_order_id INT,"
                        "sender_comp_id VARCHAR,"
                        "symbol SYMBOL,"
                        "side SYMBOL,"
                        "order_qty INT,"
                        "filled_qty INT,"
                        "ord_type SYMBOL,"
                        "price INT,"
                        "time_in_force SYMBOL,"
                        "order_status SYMBOL"
                        ") TIMESTAMP(ts) PARTITION BY DAY "
                        "DEDUP UPSERT KEYS(ts, order_id);", server_name);
            txn.exec(orders_create_sql);
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error creating orders and trades table for server {}: {}", server_name, e.what())};
        }
    }

    auto drop_quest_tables(const std::string_view& server_name)
        -> std::expected<void, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            txn.exec(std::format("DROP TABLE IF EXISTS orders_{};", server_name));
            txn.exec(std::format("DROP TABLE IF EXISTS trades_{};", server_name));
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error dropping quest db table for server {}: {}", server_name, e.what())};
        }
    }

    auto truncate_orders() -> std::expected<void, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            txn.exec("TRUNCATE TABLE orders");
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error truncating orders: {}", e.what())};
        }
    }

    auto truncate_trades() -> std::expected<void, std::string> {
        try {
            ensure_timeseries_connection();
            pqxx::work txn{*m_timeseries_db_sql_connection};
            txn.exec("TRUNCATE TABLE trades");
            txn.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error truncating trades: {}", e.what())};
        }
    }

  public:
    // These were suggested by Opus 4.6, as bulk questdb connection was causing seg fault issues.
    // These seemed to fix them, however if we want fast initial connections we want to pre-call
    // them for hot paths.
    void ensure_timeseries_connection() {
        if (!m_timeseries_db_sql_connection) {
            m_timeseries_db_sql_connection = std::make_unique<pqxx::connection>(
                "host=localhost port=8812 user=admin password=quest dbname=qdb");
        }
    }

    void ensure_async_writer() {
        if (!m_async_writer.has_value()) {
            m_async_writer.emplace(m_write_queue, m_flush_threshold, m_flush_interval);
        }
    }

  private:
    bool servers_has_initial_usd_column(pqxx::transaction_base& txn) const {
        if (m_has_servers_initial_usd_column.has_value()) {
            return m_has_servers_initial_usd_column.value();
        }
        auto res = txn.exec(
            "SELECT 1 FROM information_schema.columns "
            "WHERE table_schema = 'public' AND table_name = 'servers' "
            "AND column_name = 'initial_usd'");
        m_has_servers_initial_usd_column = !res.empty();
        return m_has_servers_initial_usd_column.value();
    }

    // Parse a PostgreSQL text-array literal (e.g. "{AAPL,GOOGL}") into a vector.
    static std::vector<std::string> parse_pg_array(const std::string& pg_array) {
        std::vector<std::string> result;
        if (pg_array.size() < 2 || pg_array.front() != '{')
            return result;
        const std::string inner = pg_array.substr(1, pg_array.size() - 2);
        if (inner.empty())
            return result;
        size_t pos = 0;
        while (pos <= inner.size()) {
            const size_t comma = inner.find(',', pos);
            const size_t end = (comma == std::string::npos) ? inner.size() : comma;
            std::string token = inner.substr(pos, end - pos);
            if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
                token = token.substr(1, token.size() - 2);
            if (!token.empty())
                result.push_back(std::move(token));
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
        return result;
    }

    // Build a PostgreSQL text-array literal from a vector of strings.
    static std::string build_pg_array(const std::vector<std::string>& items) {
        std::string arr = "{";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0)
                arr += ',';
            arr += items[i];
        }
        arr += '}';
        return arr;
    }

    // This will be shared with async writer
    ThreadSafeQueue<WriteTask> m_write_queue{};
    std::chrono::milliseconds m_flush_interval{0};
    int m_flush_threshold{0};

    // SQL-based connection for edux_core_db (PostgreSQL on port 5432).
    std::unique_ptr<pqxx::connection> m_core_db_sql_connection{
        std::make_unique<pqxx::connection>("host=localhost port=5432 dbname=edux_core_db")};

    // SQL-based connection for QuestDB (PG wire protocol on port 8812).
    // Opus suggestion: lazy init — only created when QuestDB queries are needed.
    std::unique_ptr<pqxx::connection> m_timeseries_db_sql_connection;

    // Opus suggestion: ILP async writer for QuestDB — lazily initialized on first ILP insert.
    std::optional<AsyncWriter> m_async_writer;
    mutable std::optional<bool> m_has_servers_initial_usd_column;
};

} // namespace database
