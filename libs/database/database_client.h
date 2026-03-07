#pragma once

#include <core/trade.h>
#include <core/containers.h>
#include <core/orders.h>
#include <expected>
#include <pqxx/pqxx>
#include <string>
#include <questdb/ingress/line_sender.hpp>

namespace database {

using namespace std::literals::string_view_literals;
using namespace questdb::ingress::literals;
using namespace core;

// Orders table preparsing
static const auto orders_table     = "orders"_tn;
static const auto order_id         = "order_id"_cn;
static const auto cl_order_id      = "cl_order_id"_cn;
static const auto sender_comp_id   = "sender_comp_id"_cn;
static const auto symbol           = "symbol"_cn;
static const auto side             = "side"_cn;
static const auto order_qty        = "order_qty"_cn;
static const auto filled_qty       = "filled_qty"_cn;
static const auto ord_type         = "ord_type"_cn;
static const auto price            = "price"_cn;
static const auto time_in_force    = "time_in_force"_cn;
static const auto order_status     = "order_status"_cn;

// Trades table preparsing
static const auto trades_table     = "trades"_tn;
static const auto trade_id_cn      = "trade_id"_cn;
static const auto taker_id_cn      = "taker_id"_cn;
static const auto maker_id_cn      = "maker_id"_cn;
static const auto taker_order_id_cn= "taker_order_id"_cn;
static const auto maker_order_id_cn= "maker_order_id"_cn;
static const auto quantity_cn      = "quantity"_cn;
static const auto is_taker_buyer_cn  = "is_taker_buyer"_cn;

inline std::string_view to_string(Side s) {
    switch (s) {
    case Side::bid: return "BID";
    case Side::ask: return "ASK";
    default:        return "UNKNOWN";
    }
}

inline std::string_view to_string(OrderType t) {
    switch (t) {
    case OrderType::limit:  return "LIMIT";
    case OrderType::market: return "MARKET";
    default:                return "UNKNOWN";
    }
}

inline std::string_view to_string(TimeInForce tif) {
    switch (tif) {
    case TimeInForce::day: return "DAY";
    case TimeInForce::gtc: return "GTC";
    default:               return "UNKNOWN";
    }
}

inline std::string_view to_string(ExecTypeOrOrderStatus s) {
    switch (s) {
    case OrderStatus::status_new:           return "NEW";
    case OrderStatus::status_partially_filled: return "PARTIALLY_FILLED";
    case OrderStatus::status_filled:        return "FILLED";
    case OrderStatus::status_canceled:      return "CANCELED";
    case OrderStatus::status_pending_cancel: return "PENDING_CANCEL";
    case OrderStatus::status_rejected:      return "REJECTED";
    default:                                return "UNKNOWN";
    }
}

class DatabaseClient {
  public:
    DatabaseClient() = default;

    auto read_balance(std::string user_id, std::string symbol)
        -> std::expected<int, std::string> {
        try {
            pqxx::work transaction{m_core_db_sql_connection};

            int balance{transaction.query_value<int>(
                "SELECT balance FROM balances WHERE user_id = $1 AND symbol = $2",
                pqxx::params{user_id, symbol})};

            transaction.commit();

            return balance;
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error faced when getting balance: {}", e.what())};
        }
    }

    auto update_balance(std::string user_id, std::string symbol, int balance)
        -> std::expected<void, std::string> {
        try {
            pqxx::work transaction{m_core_db_sql_connection};

            transaction.exec(
                "UPDATE balances SET balance = $3 WHERE user_id = $1 AND symbol = $2",
                pqxx::params{user_id, symbol, balance});

            transaction.commit();
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error faced when updating balance: {}", e.what())};
        }
    }

    auto insert_order(int internal_order_id,
                      const core::NewOrderSingleContainer& new_order_request)
        -> std::expected<void, std::string> {
        try {
            questdb::ingress::line_sender_buffer buffer =
                m_timeseries_db_connection.new_buffer();

            buffer.table(orders_table)
                // symbols first
                .symbol(sender_comp_id, new_order_request.sender_comp_id)
                .symbol(symbol, new_order_request.symbol)
                .symbol(side, to_string(new_order_request.side))
                .symbol(ord_type, to_string(new_order_request.ord_type))
                .symbol(time_in_force,
                        to_string(new_order_request.time_in_force))
                // columns next
                .column(order_id, static_cast<std::int64_t>(internal_order_id))
                .column(cl_order_id, static_cast<std::int64_t>(std::stoll(new_order_request.cl_ord_id)))
                .column(order_qty, static_cast<std::int64_t>(
                                       new_order_request.order_qty))
                .column(filled_qty, static_cast<std::int64_t>(0))
                // price is optional for market orders; store 0 if not present
                .column(price,
                        static_cast<std::int64_t>(new_order_request.price
                                                      ? *new_order_request.price
                                                      : 0))
                // initial order_status is NEW
                .column(order_status, std::string_view{"NEW"})
                .at(questdb::ingress::timestamp_micros::now());

            m_timeseries_db_connection.flush(buffer);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error faced when inserting order: {}", e.what())};
        }
    }

    auto insert_cancel(const core::CancelOrderRequestContainer& cancel_order_request)
        -> std::expected<void, std::string> {
        assert(cancel_order_request.order_id.has_value());
        try {
            questdb::ingress::line_sender_buffer buffer =
                m_timeseries_db_connection.new_buffer();

            // Represent cancel as an order row with status CANCELED and filled_qty = 0.
            buffer.table(orders_table)
                .symbol(symbol, cancel_order_request.symbol)
                .symbol(side, to_string(cancel_order_request.side))
                .symbol(order_status, std::string_view{"CANCELED"})
                .column(sender_comp_id, cancel_order_request.sender_comp_id)
                .column(order_id,static_cast<int64_t>(
                        std::stoll(cancel_order_request.order_id.value())))
                .column(cl_order_id, static_cast<int64_t>(std::stoll(cancel_order_request.cl_ord_id)))
                .column(order_qty, static_cast<std::int64_t>(
                                       cancel_order_request.order_qty))
                .column(filled_qty, static_cast<std::int64_t>(0))
                .at(questdb::ingress::timestamp_micros::now());

            m_timeseries_db_connection.flush(buffer);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error faced when inserting cancel: {}", e.what())};
        }
    }

    auto insert_execution(const core::ExecutionReportContainer& execution_report)
        -> std::expected<void, std::string> {
        try {
            questdb::ingress::line_sender_buffer buffer =
                m_timeseries_db_connection.new_buffer();

            // Map execution report to orders table row; use cum_qty as filled_qty and
            // ord_status from the report.
            buffer.table(orders_table)
                .symbol(symbol, execution_report.symbol)
                .symbol(side, to_string(execution_report.side))
                .symbol(order_status, to_string(execution_report.ord_status))
                .symbol(time_in_force,
                        execution_report.time_in_force
                            ? to_string(*execution_report.time_in_force)
                            : std::string_view{"UNKNOWN"})
                .column(sender_comp_id, execution_report.sender_comp_id)
                .column(order_id, static_cast<std::int64_t>(std::stoll(execution_report.order_id)))
                .column(cl_order_id, static_cast<std::int64_t>(std::stoll(execution_report.cl_order_id)))
                .column(order_qty,
                        static_cast<std::int64_t>(execution_report.leaves_qty +
                                                  execution_report.cum_qty))
                .column(filled_qty,
                        static_cast<std::int64_t>(execution_report.cum_qty))
                .column(price,
                        static_cast<std::int64_t>(
                            execution_report.price ? *execution_report.price : 0))
                .at(questdb::ingress::timestamp_micros::now());

            m_timeseries_db_connection.flush(buffer);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format(
                "Error faced when inserting execution: {}", e.what())};
        }
    }

    auto insert_trade(const Trade& trade) -> std::expected<void, std::string> {
        try {
            questdb::ingress::line_sender_buffer buffer =
                m_timeseries_db_connection.new_buffer();

            buffer.table(trades_table)
                // symbols
                .symbol(symbol, trade.ticker)
                // columns
                .column(price, static_cast<std::int64_t>(trade.price))
                .column(quantity_cn, static_cast<std::int64_t>(trade.quantity))
                .column(trade_id_cn, static_cast<std::int64_t>(trade.trade_id))
                .column(taker_id_cn, static_cast<std::int64_t>(trade.taker_id))
                .column(maker_id_cn, static_cast<std::int64_t>(trade.maker_id))
                .column(taker_order_id_cn,
                        static_cast<std::int64_t>(trade.taker_order_id))
                .column(maker_order_id_cn,
                        static_cast<std::int64_t>(trade.maker_order_id))
                .column(is_taker_buyer_cn, trade.is_taker_buyer)
                .at(questdb::ingress::timestamp_micros::now());

            m_timeseries_db_connection.flush(buffer);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error faced when inserting trade: {}", e.what())};
        }
    }

  private:
    // SQL-based connection for edux_core_db (PostgreSQL).
    pqxx::connection m_core_db_sql_connection{
        "host=localhost port=5432 dbname=edux_core_db"};

    // SQL-based connection for the questdb edux_timeseries_db.
    pqxx::connection m_timeseries_db_sql_connection{
        "host=localhost port=5432 dbname=edux_timeseries_db"};

    // Influx-based protocol (append only) for the questdb edux_timeseries_db.
    questdb::ingress::line_sender m_timeseries_db_connection{
        questdb::ingress::line_sender::from_conf(
            "tcp::addr=localhost:9000;protocol_version=3;")};
};

} // namespace database
