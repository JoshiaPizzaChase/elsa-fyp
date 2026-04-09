#pragma once

#include <memory>
#include <quickfix/FileStore.h>
#include <string>

#include "order.h"
#include "quickfix/Application.h"
#include "quickfix/Field.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/MessageCracker.h"
#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include "server_response.h"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

class FixClient : FIX::Application, FIX42::MessageCracker {
    FIX::SessionSettings _settings;
    FIX::FileStoreFactory _file_store_factory;
    std::unique_ptr<FIX::Initiator> _initiator;
    std::atomic_bool _is_connected = false;
    inline static std::atomic<int> _client_instance_counter{0};
    std::string _logger_name = "fix_client_logger_" + std::to_string(++_client_instance_counter);
    std::shared_ptr<spdlog::logger> logger = spdlog::get(_logger_name) ? spdlog::get(_logger_name) : spdlog::basic_logger_mt<spdlog::async_factory>(
        _logger_name, std::string{PROJECT_SOURCE_DIR} + "/logs/" + _logger_name + ".log");

  public:
    explicit FixClient(const std::string&);
    void connect(int) const;
    void disconnect();
    [[nodiscard]] bool is_connected() const;
    [[nodiscard]] FIX::SessionID get_session_id() const;
    // methods to interact with market
    bool submit_market_order(const std::string&, const double&, const OrderSide&, int) const;
    bool submit_limit_order(const std::string&, const double&, const double&, const OrderSide&,
                            const TimeInForce&, int) const;
    bool cancel_order(const std::string&, const OrderSide&, int, int) const;

  protected:
    virtual void on_order_update(const ExecutionReport&) = 0;
    virtual void on_order_cancel_rejected(int, const std::string&) = 0;

  private:
    void onCreate(const FIX::SessionID&) override {};
    void onLogon(const FIX::SessionID&) override {
        std::cout << "[FixClient] Connected to EduX" << '\n';
        _is_connected.store(true);
    };
    void onLogout(const FIX::SessionID&) override {
        std::cout << "[FixClient] Disconnected from EduX" << '\n';
        _is_connected.store(false);
    };
    void toAdmin(FIX::Message&, const FIX::SessionID&) override {
    }
    void toApp(FIX::Message&, const FIX::SessionID&) override {};
    void fromAdmin(const FIX::Message& msg, const FIX::SessionID& session_id) override {
        std::cout << "[FixClient] fromAdmin:" << msg << "from session " << session_id << '\n';
        crack(msg, session_id);
    };
    void fromApp(const FIX::Message& msg, const FIX::SessionID& session_id) override {
        std::cout << "[FixClient] fromApp:" << msg << "from session " << session_id << '\n';
        crack(msg, session_id);
    };
    //
    void onMessage(const FIX42::ExecutionReport&, const FIX::SessionID& session_id) override;
    void onMessage(const FIX42::OrderCancelReject&, const FIX::SessionID& session_id) override;

    // helper functions for creating requests
    static FIX42::NewOrderSingle create_new_order_fix_request(const std::string& ticker,
                                                              const double& quantity,
                                                              const OrderSide& side,
                                                              int client_order_id) {
        FIX42::NewOrderSingle new_order_fix_message;

        new_order_fix_message.set(
            FIX::Side(FIX::Side(side == OrderSide::BUY ? FIX::Side_BUY : FIX::Side_SELL)));
        new_order_fix_message.set(FIX::TransactTime(FIX::TransactTime(true)));
        new_order_fix_message.set(FIX::Symbol(ticker));
        new_order_fix_message.set(FIX::OrderQty(quantity));
        new_order_fix_message.set(FIX::ClOrdID(std::to_string(client_order_id)));

        return new_order_fix_message;
    }

    static FIX42::OrderCancelRequest create_cancel_order_fix_request(const std::string& ticker,
                                                                     const OrderSide& side,
                                                                     int orig_client_order_id,
                                                                     int client_order_id) {
        const auto orig_client_order_id_str = std::to_string(orig_client_order_id);
        const auto client_order_id_str = std::to_string(client_order_id);
        FIX42::OrderCancelRequest order_cancel_request(
            orig_client_order_id_str, client_order_id_str, ticker,
            side == OrderSide::BUY ? FIX::Side_BUY : FIX::Side_SELL, FIX::TransactTime());
        order_cancel_request.set(FIX::OrderQty(0));
        return order_cancel_request;
    }
};
