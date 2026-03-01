#pragma once

#include "core/trade.h"
#include <core/containers.h>
#include <expected>
#include <pqxx/pqxx>
#include <string>
#include <questdb/ingress/line_sender.hpp>

namespace database {

class DatabaseClient {
  public:
    DatabaseClient() = default;

    auto query_balance(std::string user_id, std::string symbol) -> std::expected<int, std::string> {
        try {
            pqxx::work transaction{m_core_db_connection_string};

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
            pqxx::work transaction{m_core_db_connection_string};

            transaction.exec("UPDATE balances SET balance = $3 WHERE user_id = $1 AND symbol = $2",
                             pqxx::params{user_id, symbol, balance});

            return {};
        } catch (const std::exception& e) {
            return std::unexpected{std::format("Error faced when updating balance: {}", e.what())};
        }
    }

    auto insert_order(const core::NewOrderSingleContainer& new_order_request) -> std::expected<void, std::string> {

    }

    auto update_order(const core::CancelOrderRequestContainer& cancel_order_request) -> std::expected<void, std::string> {

    }

    auto update_order(const core::ExecutionReportContainer& execution_report) -> std::expected<void, std::string> {

    }

    auto insert_trade(const Trade& trade) -> std::expected<void, std::string> {

    }

  private:
    pqxx::connection m_core_db_connection_string{"host=localhost port=5432 dbname=edux_core_db"};
    // TODO: I think we need to have the env. var set.

};
} // namespace database
