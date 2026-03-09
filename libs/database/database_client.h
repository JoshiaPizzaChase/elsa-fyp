#pragma once

#include <chrono>
#include <core/containers.h>
#include <core/orders.h>
#include <core/thread_safe_queue.h>
#include <core/trade.h>
#include <expected>
#include <optional>
#include <format>
#include <memory>
#include <atomic>
#include <pqxx/pqxx>
#include <questdb/ingress/line_sender.hpp>
#include <string>
#include <thread>
#include <vector>


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
            const auto orders_table = "orders"_tn;
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

            m_buffer
                .table(orders_table)
                // symbols first
                .symbol(sender_comp_id, new_order_request.sender_comp_id)
                .symbol(symbol, new_order_request.symbol)
                .symbol(side, to_string(new_order_request.side))
                .symbol(ord_type, to_string(new_order_request.ord_type))
                .symbol(time_in_force, to_string(new_order_request.time_in_force))
                // columns next
                .column(order_id, static_cast<std::int64_t>(order.internal_order_id))
                .column(cl_order_id,
                        static_cast<std::int64_t>(std::stoll(new_order_request.cl_ord_id)))
                .column(order_qty, static_cast<std::int64_t>(new_order_request.order_qty))
                .column(filled_qty, static_cast<std::int64_t>(0))
                // price is optional for market orders; store 0 if not present
                .column(price, static_cast<std::int64_t>(
                                   new_order_request.price ? *new_order_request.price : 0))
                // initial order_status is NEW
                .column(order_status, std::string_view{"NEW"})
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
            const auto orders_table = "orders"_tn;
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
            const auto orders_table = "orders"_tn;
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
            const auto trades_table = "trades"_tn;
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
    auto read_balance(std::string user_id, std::string symbol) -> std::expected<int, std::string> {
        try {
            pqxx::work transaction{*m_core_db_sql_connection};

            int balance{transaction.query_value<int>(
                "SELECT balance FROM balances WHERE user_id = $1 AND symbol = $2",
                pqxx::params{user_id, symbol})};

            transaction.commit();

            return balance;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when getting balance: {}", e.what())};
        }
    }

    // This is called periodically (e.g. hourly) by the OM so it should be performed asynchronously.
    auto update_balance(std::string user_id, std::string symbol, int balance)
        -> std::expected<void, std::string> {
        try {
            pqxx::work transaction{*m_core_db_sql_connection};

            transaction.exec("UPDATE balances SET balance = $3 WHERE user_id = $1 AND symbol = $2",
                             pqxx::params{user_id, symbol, balance});

            transaction.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when updating balance: {}", e.what())};
        }
    }

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
};

} // namespace database
