#pragma once
#include "database/database_client.h"
#include "order_manager_database.h"

namespace om {
class DatabaseClientWrapper : public OrderManagerDatabase {
  public:
    explicit DatabaseClientWrapper(bool ensure_init = true) : client{ensure_init} {
    }

    std::expected<int, std::string> ensure_initial_usd_balances(std::string_view server_name,
                                                                int initial_usd_amount) override {
        return client.ensure_initial_usd_balances(server_name, initial_usd_amount);
    }

    std::expected<std::vector<DbUserBalanceInfo>, std::string>
    get_all_users_balances_for_server(std::string_view server_name) override {
        return client.get_all_users_balances_for_server(server_name)
            .transform([](std::vector<database::DatabaseClient::UserBalanceInfo>&& user_bal_info)
                           -> std::vector<DbUserBalanceInfo> {
                std::vector<DbUserBalanceInfo> db_user_balance_infos;
                db_user_balance_infos.reserve(user_bal_info.size());

                for (const auto& [user_id, username, balances] : user_bal_info) {
                    DbUserBalanceInfo bal_info{.user_id = user_id, .username = username};
                    bal_info.balances.reserve(balances.size());
                    for (const auto& [symbol, balance] : balances) {
                        bal_info.balances.emplace_back(symbol, balance);
                    }
                    db_user_balance_infos.push_back(std::move(bal_info));
                }

                return db_user_balance_infos;
            })
            .or_else([](std::string&& err)
                         -> std::expected<std::vector<DbUserBalanceInfo>, std::string> {
                return std::unexpected{err};
            });
    }

    std::expected<void, std::string> insert_order(int internal_order_id,
                                                  const core::NewOrderSingleContainer& new_order,
                                                  bool is_order_valid) override {
        return client.insert_order(internal_order_id, new_order, is_order_valid);
    }

    std::expected<void, std::string>
    insert_cancel_request(const core::CancelOrderRequestContainer& cancel_request,
                          bool is_request_valid) override {
        return client.insert_cancel_request(cancel_request, is_request_valid);
    }

    std::expected<void, std::string> insert_trade(const core::TradeContainer& trade) override {
        return client.insert_trade(trade);
    }

    std::expected<void, std::string>
    insert_cancel_response(const core::CancelOrderResponseContainer& cancel_response) override {
        return client.insert_cancel_response(cancel_response);
    }

  private:
    database::DatabaseClient client;
};
} // namespace om