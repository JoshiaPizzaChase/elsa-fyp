#pragma once

#include "core/containers.h"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace om {

struct DbBalanceRow {
    std::string symbol;
    std::int64_t balance{};
};

struct DbUserBalanceInfo {
    int user_id;
    std::string username;
    std::vector<DbBalanceRow> balances;
};

struct DbServerRow {
    int server_id{};
    int admin_id{};
    std::string server_name;
    std::string admin_name;
    std::vector<std::string> active_tickers;
    std::string description;
    int initial_usd{100000};
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

    virtual std::expected<std::optional<DbServerRow>, std::string>
    get_server(const std::string_view& server_name) = 0;

    virtual std::expected<void, std::string>
    update_balance(int user_id, int server_id, std::string_view symbol, std::int64_t balance) = 0;
};
} // namespace om
