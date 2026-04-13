#pragma once

#include "balance_checker.h"
#include "core/containers.h"
#include "order_manager_database.h"
#include "transport/inbound_server.h"
#include "transport/outbound_client.h"
#include "websocket_client.h"

#include <boost/bimap.hpp>

namespace om {
inline const std::string USD_SYMBOL = "USD";

struct OrderInfo {
    std::string sender_comp_id;
    std::string symbol;
    core::Side side;
    std::optional<int> price;
    core::TimeInForce time_in_force;
    int leaves_qty;
    int cum_qty;
    int avg_px;

    int arrival_gateway_id; // Assume that all orders in same order_id chain arrives in same gateway
};

struct OrderManagerDependencyFactory {
    std::function<std::unique_ptr<transport::InboundServer>(
        std::string_view, int, std::shared_ptr<spdlog::logger>, std::vector<int>&)>
        create_inbound_server;

    std::function<std::unique_ptr<transport::OutboundClient>(std::shared_ptr<spdlog::logger>)>
        create_outbound_client;

    std::function<std::unique_ptr<OrderManagerDatabase>(bool)> create_database_client;
};

class OrderManager {
  public:
    OrderManager(std::string_view host, int port,
                 const OrderManagerDependencyFactory& dependency_factory);
    void init();
    void connect_matching_engine(std::string host, int port, int try_attempts = 5);
    [[noreturn]] void start();

    using OrderIdMapContainer = boost::bimap<int, int>;
    using OrderIdPair = OrderIdMapContainer::value_type;
    using OrderInfoMapContainer = std::unordered_map<int, OrderInfo>;
    using UsernameToUserIdMapContainer = std::unordered_map<std::string, int>;

  private:
    std::vector<int> gateway_connection_ids;
    int order_request_connection_id;
    int order_response_connection_id;

    std::unique_ptr<transport::InboundServer> inbound_server;
    std::unique_ptr<transport::OutboundClient> order_request_outbound_client;
    std::unique_ptr<transport::OutboundClient> order_response_outbound_client;

    BalanceChecker balance_checker;
    UsernameToUserIdMapContainer username_user_id_map;

    std::unique_ptr<OrderManagerDatabase> database_client;

    // Left is internal order ID, Right is sender id + client order ID (for preventing a user
    // having duplicate client order ID)
    OrderIdMapContainer order_id_map;

    OrderInfoMapContainer order_info_map;
};

void init_balance_checker(BalanceChecker& balance_checker,
                          OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                          OrderManagerDatabase& database_client);

[[nodiscard]] std::expected<std::optional<int>, std::string>
preprocess_container(core::Container& container, OrderManager::OrderIdMapContainer& order_id_map,
                     OrderManager::OrderInfoMapContainer& order_info_map,
                     const OrderManager::UsernameToUserIdMapContainer& username_user_id_map,
                     int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                     int order_request_connection_id);

std::string validate_container(const core::Container& container, BalanceChecker& balance_checker,
                               std::optional<int> fill_cost = std::nullopt);

void forward_and_reply(bool is_container_valid, const core::Container& container,
                       const OrderManager::OrderInfoMapContainer& order_info_map,
                       int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                       int order_request_connection_id, transport::InboundServer& inbound_ws_server,
                       std::optional<std::string_view> order_reject_reason = std::nullopt);

core::ExecutionReportContainer
generate_rejection_report_container(const core::Container& container,
                                    const OrderManager::OrderInfoMapContainer& order_info_map);

core::ExecutionReportContainer
generate_success_report_container(const core::Container& container,
                                  const OrderManager::OrderInfoMapContainer& order_info_map);

void update_database(const core::Container& container, OrderManagerDatabase& database_client,
                     std::optional<bool> valid_container = std::nullopt);

void update_order_info(const core::TradeContainer& trade_container,
                       OrderManager::OrderInfoMapContainer& order_info_map);

void return_execution_report(const core::Container& container,
                             const OrderManager::OrderIdMapContainer& order_id_map,
                             const OrderManager::OrderInfoMapContainer& order_info_map,
                             transport::InboundServer& inbound_ws_server,
                             OrderManagerDatabase& database_client);

std::pair<core::ExecutionReportContainer, core::ExecutionReportContainer>
generate_matched_order_report_containers(const core::TradeContainer& trade,
                                         const OrderManager::OrderIdMapContainer& order_id_map,
                                         const OrderManager::OrderInfoMapContainer& order_info_map);

core::ExecutionReportContainer generate_cancel_response_report_container(
    const core::CancelOrderResponseContainer& cancel_response,
    const OrderManager::OrderIdMapContainer& order_id_map,
    const OrderManager::OrderInfoMapContainer& order_info_map);
} // namespace om
