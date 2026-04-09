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
        auto raw = client.get_all_users_balances_for_server(server_name);
        if (!raw.has_value())
            return std::unexpected(raw.error());

        std::vector<DbUserBalanceInfo> mapped;
        mapped.reserve(raw.value().size());
        for (const auto& u : raw.value()) {
            DbUserBalanceInfo out{.username = u.username};
            out.balances.reserve(u.balances.size());
            for (const auto& b : u.balances) {
                out.balances.push_back({.symbol = b.symbol, .balance = b.balance});
            }
            mapped.push_back(std::move(out));
        }
        return mapped;
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