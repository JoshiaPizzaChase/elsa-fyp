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
    std::function<std::unique_ptr<transport::InboundServer>(std::string_view, int,
                                                            std::shared_ptr<spdlog::logger>)>
        create_inbound_server;

    std::function<std::unique_ptr<transport::OutboundClient>(std::shared_ptr<spdlog::logger>)>
        create_outbound_client;

    std::function<std::unique_ptr<OrderManagerDatabase>(bool)> create_database_client;
};

class OrderManager {
  public:
    OrderManager(std::string_view host, int port, int gateway_count,
                 const OrderManagerDependencyFactory& dependency_factory);
    void init();
    std::expected<void, std::string> connect_matching_engine(std::string host, int port);
    std::expected<void, std::string> start();

    typedef boost::bimap<int, int>::value_type order_id_pair;

  private:
    std::unique_ptr<transport::InboundServer> inbound_server;
    std::unique_ptr<transport::OutboundClient> order_request_outbound_client;
    std::unique_ptr<transport::OutboundClient> order_response_outbound_client;
    BalanceChecker balance_checker;
    std::unique_ptr<OrderManagerDatabase> database_client;

    int gateway_count;
    int order_request_connection_id;
    int order_response_connection_id;

    // Left is internal order ID, Right is client order ID
    boost::bimap<int, int> order_id_map;

    std::unordered_map<int, OrderInfo> order_info_map;
};

std::optional<int> preprocess_container(core::Container& container,
                                        boost::bimap<int, int>& order_id_map,
                                        std::unordered_map<int, OrderInfo>& order_info_map,
                                        int arrival_gateway_id,
                                        transport::OutboundClient& order_request_ws_client,
                                        int order_request_connection_id);

bool validate_container(const core::Container& container, BalanceChecker& balance_checker,
                        std::optional<int> fill_cost = std::nullopt);

void forward_and_reply(bool is_container_valid, const core::Container& container,
                       const std::unordered_map<int, OrderInfo>& order_info_map,
                       int arrival_gateway_id, transport::OutboundClient& order_request_ws_client,
                       int order_request_connection_id, transport::InboundServer& inbound_ws_server,
                       OrderManagerDatabase& database_client);

core::ExecutionReportContainer
generate_rejection_report_container(const core::Container& container,
                                    const std::unordered_map<int, OrderInfo>& order_info_store);

core::ExecutionReportContainer
generate_success_report_container(const core::Container& container,
                                  const std::unordered_map<int, OrderInfo>& order_info_store);

void update_database(const core::Container& container, OrderManagerDatabase& database_client,
                     std::optional<bool> valid_container = std::nullopt);

void update_order_info(const core::TradeContainer& trade_container,
                       std::unordered_map<int, OrderInfo>& order_info_map);

void return_execution_report(const core::Container& container,
                             const boost::bimap<int, int>& order_id_map,
                             const std::unordered_map<int, OrderInfo>& order_info_map,
                             transport::InboundServer& inbound_ws_server,
                             OrderManagerDatabase& database_client);

std::pair<core::ExecutionReportContainer, core::ExecutionReportContainer>
generate_matched_order_report_containers(const core::TradeContainer& trade,
                                         const boost::bimap<int, int>& order_id_map,
                                         const std::unordered_map<int, OrderInfo>& order_info_map);

core::ExecutionReportContainer
generate_cancel_response_report_container(const core::CancelOrderResponseContainer& cancel_response,
                                          const boost::bimap<int, int>& order_id_map,
                                          const std::unordered_map<int, OrderInfo>& order_info_map);
} // namespace om
