#pragma once
#include "quickfix/Application.h"
#include "quickfix/Session.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/MessageCracker.h"
#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include <iostream>

class TestFixServer final : public FIX::Application, FIX42::MessageCracker {
  public:
    void onCreate(const FIX::SessionID&) override {};
    void onLogon(const FIX::SessionID&) override {
        std::cout << "[Server] logon received" << '\n';
    };
    void onLogout(const FIX::SessionID&) override {
        std::cout << "[Server] logout received" << '\n';
    };
    void toAdmin(FIX::Message&, const FIX::SessionID&) override {
    }
    void toApp(FIX::Message&, const FIX::SessionID&) override {};
    void fromAdmin(const FIX::Message& msg, const FIX::SessionID& session_id) override {
        crack(msg, session_id);
    };
    void fromApp(const FIX::Message& msg, const FIX::SessionID& session_id) override {
        crack(msg, session_id);
    };

  private:
    void onMessage(const FIX42::NewOrderSingle& new_order,
                   const FIX::SessionID& session_id) override {
        std::cout << "[Server] received new order request:\n"
                  << new_order << "\nfrom session: " << session_id << '\n';
        FIX42::ExecutionReport execution_report{};
        FIX::ClOrdID client_order_id;
        FIX::Symbol symbol;
        FIX::Side side;
        FIX::OrderQty order_qty;
        FIX::OrdType ord_type;
        new_order.get(client_order_id);
        new_order.get(symbol);
        new_order.get(side);
        new_order.get(order_qty);
        new_order.get(ord_type);

        if (FIX::Price price; ord_type == FIX::OrdType_LIMIT && new_order.isSet(price)) {
            new_order.get(price);
            execution_report.set(FIX::LastPx(price));
            execution_report.set(FIX::AvgPx(price));
        } else {
            execution_report.set(FIX::LastPx(1.0)); // for testing only
            execution_report.set(FIX::AvgPx(1.0));
        }
        execution_report.set(client_order_id);
        execution_report.set(symbol);
        execution_report.set(FIX::OrdStatus(FIX::OrdStatus_FILLED));
        execution_report.set(side);
        execution_report.set(order_qty);
        execution_report.set(FIX::ExecType(FIX::ExecType_FILL));
        execution_report.set(FIX::CumQty(order_qty.getValue()));
        execution_report.set(FIX::LeavesQty(0.0));
        execution_report.set(FIX::OrderID("OrderIdFromServer"));
        FIX::Session::sendToTarget(execution_report, session_id);
    }
    void onMessage(const FIX42::OrderCancelRequest& cancel_request,
                   const FIX::SessionID& session_id) override {
        std::cout << "[Server] received cancel order request:\n"
                  << cancel_request << "\nfrom session: " << session_id << '\n';
        FIX42::OrderCancelReject order_cancel_reject{};
        FIX::ClOrdID client_order_id;
        cancel_request.get(client_order_id);
        order_cancel_reject.set(client_order_id);
        order_cancel_reject.set(
            FIX::Text("Server rejected cancellation because the developer is testing"));
        FIX::Session::sendToTarget(order_cancel_reject, session_id);
    }
};
