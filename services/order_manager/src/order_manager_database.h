#pragma once

#include "core/containers.h"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace om {

struct DbBalanceRow {
    std::string symbol;
    int balance{};
};

struct DbUserBalanceInfo {
    std::string username;
    std::vector<DbBalanceRow> balances;
};

class OrderManagerDatabase {
  public:
    virtual ~OrderManagerDatabase() = default;

    virtual std::expected<int, std::string>
    ensure_initial_usd_balances(std::string_view server_name, int initial_usd_amount) = 0;

    virtual std::expected<std::vector<DbUserBalanceInfo>, std::string>
    get_all_users_balances_for_server(std::string_view server_name) = 0;

    virtual std::expected<void, std::string>
    insert_order(int internal_order_id, const core::NewOrderSingleContainer& new_order,
                 bool is_order_valid) = 0;

    virtual std::expected<void, std::string>
    insert_cancel_request(const core::CancelOrderRequestContainer& cancel_request,
                          bool is_request_valid) = 0;

    virtual std::expected<void, std::string> insert_trade(const core::TradeContainer& trade) = 0;

    virtual std::expected<void, std::string>
    insert_cancel_response(const core::CancelOrderResponseContainer& cancel_response) = 0;
};
} // namespace om