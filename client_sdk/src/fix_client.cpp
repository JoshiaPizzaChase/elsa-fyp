#include "fix_client.h"

#include "quickfix/FileStore.h"
#include "quickfix/Session.h"
#include <chrono>
#include <thread>

FixClient::FixClient(const std::string& setting_file)
    : Application(), _settings{setting_file}, _file_store_factory{_settings} {
    _initiator = std::unique_ptr<FIX::Initiator>(
        new FIX::SocketInitiator(*this, _file_store_factory, _settings));
}

void FixClient::connect(const int& timeout_sec) const {
    logger->info("Connecting to EduX...");
    _initiator->start();
    std::chrono::seconds timeout(timeout_sec);
    auto start = std::chrono::steady_clock::now();
    while (!_is_connected.load(std::memory_order_relaxed)) {
        auto now = std::chrono::steady_clock::now();
        if (now - start > timeout) {
            throw std::runtime_error("Connection timed out");
        }
        // sleep shortly to prevent spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void FixClient::disconnect() {
    _initiator->stop();
    logger->info("Disconnected from EduX");
    _is_connected.store(false);
}

bool FixClient::is_connected() const {
    const bool res = _is_connected.load(std::memory_order_relaxed);
    if (!res) {
        logger->error("Not connected to FIX server");
    }
    return res;
}

bool FixClient::submit_market_order(const std::string& ticker, const double& quantity,
                                    const OrderSide& side,
                                    const std::string& custom_order_id) const {
    if (!is_connected())
        return false;
    try {
        FIX42::NewOrderSingle new_order_fix_message =
            create_new_order_fix_request(ticker, quantity, side, custom_order_id);
        new_order_fix_message.set(FIX::OrdType(FIX::OrdType_MARKET));
        FIX::Session::sendToTarget(new_order_fix_message, get_session_id());
        logger->info("[FixClient] Market Order submitted: {}", new_order_fix_message.toString());
        return true;
    } catch (const std::exception& e) {
        logger->error("[FixClient] Failed to submit market order: {}", e.what());
        return false;
    }
}

bool FixClient::submit_limit_order(const std::string& ticker, const double& price,
                                   const double& quantity, const OrderSide& side,
                                   const TimeInForce& time_in_force,
                                   const std::string& custom_order_id) const {
    if (!is_connected())
        return false;
    try {
        logger->info("[FixClient] Submitting limit order: {} {} {}@{}",
                     side == OrderSide::BUY ? "BUY " : "SELL ", quantity, ticker, price);

        FIX42::NewOrderSingle new_order_fix_message =
            create_new_order_fix_request(ticker, quantity, side, custom_order_id);
        char tif;
        if (time_in_force == TimeInForce::GTC) {
            tif = FIX::TimeInForce_GOOD_TILL_CANCEL;
        } else {
            throw std::invalid_argument("Only GTC is supported for limit order currently!");
        }
        new_order_fix_message.set(FIX::TimeInForce(tif));
        // leave price check for server side
        new_order_fix_message.set(FIX::Price(price));
        new_order_fix_message.set(FIX::OrdType(FIX::OrdType_LIMIT));
        FIX::Session::sendToTarget(new_order_fix_message, get_session_id());
        logger->info("[FixClient] Limit Order submitted: {}", new_order_fix_message.toString());
        return true;
    } catch (const std::exception& e) {
        logger->error("[FixClient] Failed to submit limit order: {}", e.what());
        return false;
    }
}

bool FixClient::cancel_order(const std::string& ticker, const OrderSide& side,
                             const std::string& custom_order_id) const {
    if (!is_connected())
        return false;
    try {
        FIX42::OrderCancelRequest cancel_request =
            create_cancel_order_fix_request(ticker, side, custom_order_id);
        FIX::Session::sendToTarget(cancel_request, get_session_id());
        logger->info("[FixClient] Cancel order request submitted: {}", cancel_request.toString());
        return true;
    } catch (const std::exception& e) {
        logger->error("[FixClient] Failed to cancel order: {}", e.what());
        return false;
    }
}

FIX::SessionID FixClient::get_session_id() const {
    // Get the first session from settings
    std::set<FIX::SessionID> sessions = _initiator->getSessions();
    if (sessions.empty()) {
        throw std::runtime_error("No sessions configured");
    }

    return *sessions.begin();
}

// callback on order update
void FixClient::onMessage(const FIX42::ExecutionReport& execution_report,
                          const FIX::SessionID& session_id) {
    FIX::OrderID order_id;
    FIX::ClOrdID client_order_id;
    FIX::ExecType exec_type;
    FIX::OrdStatus order_status;
    FIX::Symbol symbol;
    FIX::Side side;
    FIX::OrderQty order_qty; // filled qty in this single update
    FIX::LastPx last_px;
    FIX::CumQty cum_qty; // cumulated filled qty
    FIX::AvgPx avg_px;
    FIX::LeavesQty leaves_qty;

    execution_report.get(order_id);
    execution_report.get(client_order_id);
    execution_report.get(exec_type);
    execution_report.get(order_status);
    execution_report.get(symbol);
    execution_report.get(side);
    execution_report.get(order_qty);
    execution_report.get(last_px);
    execution_report.get(cum_qty);
    execution_report.get(leaves_qty);
    execution_report.get(avg_px);

    ExecutionReport exec_report;
    exec_report.order_id = order_id;
    exec_report.custom_order_id = client_order_id;
    exec_report.ticker = symbol;
    exec_report.side = side == FIX::Side_BUY ? OrderSide::BUY : OrderSide::SELL;
    exec_report.filled_qty = order_qty;
    exec_report.last_px = last_px;
    exec_report.cumulated_filled_qty = cum_qty;
    exec_report.remaining_qty = leaves_qty;

    switch (order_status) {
    case (FIX::OrdStatus_NEW): {
        exec_report.status = OrderStatus::NEW;
        break;
    }
    case (FIX::OrdStatus_FILLED): {
        exec_report.status = OrderStatus::FILLED;
    }
    case (FIX::OrdStatus_PARTIALLY_FILLED): {
        exec_report.status = OrderStatus::PARTIALLY_FILLED;
    }
    case (FIX::OrdStatus_CANCELED): {
        exec_report.status = OrderStatus::CANCELED;
        break;
    }
    default: {
        logger->info("[FixClient] Unsupported order status received: {}", order_status.getString());
    };
    }
    on_order_update(exec_report);
}

// callback on cancel rejected
void FixClient::onMessage(const FIX42::OrderCancelReject& order_cancel_reject_response,
                          const FIX::SessionID& session_id) {
    FIX::ClOrdID client_order_id;
    FIX::Text reason;
    order_cancel_reject_response.get(client_order_id);
    order_cancel_reject_response.get(reason);
    logger->info("[FixClient] cancel request custom_order_id={} rejected, reason={}",
                 client_order_id.getString(), reason.getString());

    on_order_cancel_rejected(client_order_id, reason);
}
