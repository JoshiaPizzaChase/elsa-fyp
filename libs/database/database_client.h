#pragma once

#include "../../deps/c-questdb-client/include/questdb/ingress/line_sender.hpp"
#include "core/trade.h"
#include <core/containers.h>
#include <expected>
#include <pqxx/pqxx>
#include <string>
namespace database {

using namespace std::literals::string_view_literals;
using namespace questdb::ingress::literals;

static const auto orders_table = "orders"_tn;
static const auto order_id = "order_id"_cn;
static const auto cl_order_id = "cl_order_id"_cn;
static const auto sender_comp_id = "sender_comp_id"_cn;
static const auto symbol = "symbol"_cn;
static const auto side = "side"_cn;
static const auto order_qty = "order_qty"_cn;
static const auto filled_qty = "filled_qty"_cn;
static const auto price = "price"_cn;
static const auto time_in_force = "time_in_force"_cn;
static const auto order_status = "order_status"_cn;

class DatabaseClient {
  public:
    // TODO: Add prepared statements in constructor.
    DatabaseClient() = default;

    auto query_balance(std::string user_id, std::string symbol) -> std::expected<int, std::string> {
        try {
            pqxx::work transaction{m_core_db_connection};

            int balance{transaction.query_value<int>(
                "SELECT balance FROM balances WHERE user_id = $1 AND symbol = $2",
                pqxx::params{user_id, symbol})};

            transaction.commit();

            return balance;
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when getting balance: {}", e.what())};
        }
    }

    auto update_balance(std::string user_id, std::string symbol, int balance)
        -> std::expected<void, std::string> {
        try {
            pqxx::work transaction{m_core_db_connection};

            transaction.exec("UPDATE balances SET balance = $3 WHERE user_id = $1 AND symbol = $2",
                             pqxx::params{user_id, symbol, balance});


        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when updating balance: {}", e.what())};
        }
    }

    auto insert_order(int internal_order_id, const core::NewOrderSingleContainer& new_order_request) -> std::expected<void, std::string> {
        questdb::ingress::line_sender_buffer buffer = m_timeseries_db_connection.new_buffer();

        buffer.table(orders_table)
        .at(questdb::ingress::timestamp_micros::now());

        m_timeseries_db_connection.flush(buffer);
    }

    /*
     * Retrieve internal order_id from map if client did not provide order_id in request.
     */
    auto insert_cancel(const core::CancelOrderRequestContainer& cancel_order_request)
        -> std::expected<void, std::string> {
        assert(cancel_order_request.order_id.has_value());
        questdb::ingress::line_sender_buffer buffer = m_timeseries_db_connection.new_buffer();

        buffer.table(orders_table)
            .at(questdb::ingress::timestamp_micros::now());

        m_timeseries_db_connection.flush(buffer);
        return {};
    }

    auto insert_execution(const core::ExecutionReportContainer& execution_report)
        -> std::expected<void, std::string> {
        questdb::ingress::line_sender_buffer buffer = m_timeseries_db_connection.new_buffer();

        buffer.table(orders_table)
            .at(questdb::ingress::timestamp_micros::now());

        m_timeseries_db_connection.flush(buffer);
        return {};
    }

    auto insert_trade(const Trade& trade) -> std::expected<void, std::string> {
        questdb::ingress::line_sender_buffer buffer = m_timeseries_db_connection.new_buffer();

        const auto trades_table = "trade"_tn;

        buffer.table(trades_table)
        .at(questdb::ingress::timestamp_micros::now());

        m_timeseries_db_connection.flush(buffer);
        return {};
    }

  private:
    // Currently hardcoded.
    pqxx::connection m_core_db_connection{"host=localhost port=5432 dbname=edux_core_db"};
    questdb::ingress::line_sender m_timeseries_db_connection {questdb::ingress::line_sender::from_conf("tcp::addr=localhost:9000;protocol_version=3;")};

};
} // namespace database
