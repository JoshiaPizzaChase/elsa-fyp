#ifndef TRANSPORT_MESSAGING_H
#define TRANSPORT_MESSAGING_H

#include "core/containers.h"
#include "protobufs/containers.pb.h"
#include <memory>
#include <print>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <websocketpp/client.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

namespace transport {

// TODO: Refactor the connection and underlying websocket endpoint functionality to another file
// "websocket.h"
// TODO: Why pass handle by value then move? Why not const ref?
using Client = websocketpp::client<websocketpp::config::asio_client>;

class ConnectionMetadata {
  public:
    using connMetaSharedPtr = websocketpp::lib::shared_ptr<ConnectionMetadata>;

    ConnectionMetadata(int id, websocketpp::connection_hdl handle, std::string_view uri)
        : m_id(id), m_handle(std::move(handle)), m_status("Connecting"), m_uri(uri),
          m_server("N/A") {
    }

    // Handlers
    // TODO: Maybe convert to non-const reference instead of raw pointers... not sure why the
    // getters are non-const methods.
    void onOpen(Client* client, websocketpp::connection_hdl handle) {
        m_status = "Open";

        Client::connection_ptr connection = client->get_con_from_hdl(std::move(handle));
        m_server = connection->get_response_header("Server");
    }

    void onFail(Client* client, websocketpp::connection_hdl handle) {
        m_status = "Failed";

        Client::connection_ptr connection = client->get_con_from_hdl(std::move(handle));
        m_server = connection->get_response_header("Server");
        m_errorReason = connection->get_ec().message();
    }

    void onClose(Client* client, websocketpp::connection_hdl handle) {
        m_status = "Closed";
        Client::connection_ptr connection = client->get_con_from_hdl(std::move(handle));
        std::stringstream s;
        s << "close code: " << connection->get_remote_close_code() << " ("
          << websocketpp::close::status::get_string(connection->get_remote_close_code())
          << "), close reason: " << connection->get_remote_close_reason();
        m_errorReason = s.str();
    }

    void onMessage(websocketpp::connection_hdl handle, Client::message_ptr msg) {
        throw std::runtime_error("unimplemented");
    }

    [[nodiscard]] websocketpp::connection_hdl getHandle() const {
        return m_handle;
    }

    [[nodiscard]] int getId() const {
        return m_id;
    }

    [[nodiscard]] std::string getStatus() const {
        return m_status;
    }

    void recordSentMessage(std::string_view message) {
        m_messageStore.emplace_back(message);
    }

    friend std::ostream& operator<<(std::ostream& out, ConnectionMetadata const& data) {
        out << "> URI: " << data.m_uri << "\n"
            << "> Status: " << data.m_status << "\n"
            << "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server)
            << "\n"
            << "> Error/close reason: "
            << (data.m_errorReason.empty() ? "N/A" : data.m_errorReason);

        out << "> Messages Processed: (" << data.m_messageStore.size() << ") \n";

        for (const auto& message : data.m_messageStore) {
            out << message << "\n";
        }

        return out;
    }

  private:
    int m_id;
    websocketpp::connection_hdl m_handle;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_errorReason;
    std::vector<std::string> m_messageStore;
};

// TODO: Probably good to have some kind of "abstract base class/interface" for messaging clients
// and servers (ie inter-service messaging). Later.
// Note: Such client/servers should not have to deal with specific transport protocols (eg gRPC, raw
// TCP etc).
// Aha! *If* we have some "Serializer" interface, with multiple
// impl, we can inject different serializers! *But* we don't have multiple serializers yet, so using
// free function approach whenever possible is better.
class OrderManagerClient {
    // TODO: complete this class. To be used by gateway.

  public:
    OrderManagerClient() {
        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        // Launches a new thread
        m_thread = std::make_shared<websocketpp::lib::thread>(&Client::run, &m_endpoint);
    }

    ~OrderManagerClient() {
        m_endpoint.stop_perpetual();

        for (const auto& it : m_connectionMap) {
            if (it.second->getStatus() != "Open") {
                // Only close open connections
                continue;
            }

            std::println("Closing connection: {}", it.second->getId());

            websocketpp::lib::error_code errorCode;
            m_endpoint.close(it.second->getHandle(), websocketpp::close::status::going_away, "",
                             errorCode);
            if (errorCode) {
                std::println("Error closing connection {}: {}", it.second->getId(),
                             errorCode.message());
            }
        }

        m_thread->join();
    }

    int connect(std::string const& uri) {
        websocketpp::lib::error_code errorCode;

        Client::connection_ptr connection = m_endpoint.get_connection(uri, errorCode);

        if (errorCode) {
            std::println("Connect initialization error: {}", errorCode.message());
            return -1;
        }

        int new_id = m_next_id++;
        ConnectionMetadata::connMetaSharedPtr metadata_ptr(
            new ConnectionMetadata(new_id, connection->get_handle(), uri));
        m_connectionMap[new_id] = metadata_ptr;

        connection->set_open_handler([metadata_ptr, capture0 = &m_endpoint](auto&& PH1) {
            metadata_ptr->onOpen(capture0, std::forward<decltype(PH1)>(PH1));
        });
        connection->set_fail_handler([metadata_ptr, capture0 = &m_endpoint](auto&& PH1) {
            metadata_ptr->onFail(capture0, std::forward<decltype(PH1)>(PH1));
        });
        connection->set_message_handler([metadata_ptr](auto&& PH1, auto&& PH2) {
            metadata_ptr->onMessage(std::forward<decltype(PH1)>(PH1),
                                    std::forward<decltype(PH2)>(PH2));
        });

        m_endpoint.connect(connection);

        return new_id;
    }

    void close(int id, websocketpp::close::status::value code) {
        websocketpp::lib::error_code errorCode;

        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            std::println("No connection found with id {}", id);
            return;
        }

        m_endpoint.close(metadata_it->second->getHandle(), code, "", errorCode);
        if (errorCode) {
            std::println("Error initiating close: {}", errorCode.message());
        }
    }

    void send(int id, std::string message) {
        websocketpp::lib::error_code errorCode;

        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            std::println("No connection found with id {}", id);
            return;
        }

        m_endpoint.send(metadata_it->second->getHandle(), message,
                        websocketpp::frame::opcode::binary, errorCode);
        if (errorCode) {
            std::println("Error sending message: {}", errorCode.message());

            return;
        }

        metadata_it->second->recordSentMessage(message);
    }

    ConnectionMetadata::connMetaSharedPtr get_metadata(int id) const {
        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            return {};
        }

        return metadata_it->second;
    }

  private:
    using ConnectionMap = std::unordered_map<int, ConnectionMetadata::connMetaSharedPtr>;
    Client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    ConnectionMap m_connectionMap;
    int m_next_id{0};
};

class OrderManagerServer {
    // TODO: complete this class. To be used by order manager.
  public:
  private:
};

// TODO: Improve these conversion functions. Is there any way to avoid boilerplate?
inline transport::Side convertToProto(core::Side side) {
    switch (side) {
    case core::Side::bid:
        return transport::Side::SIDE_BID;
    case core::Side::ask:
        return transport::Side::SIDE_ASK;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

inline transport::OrderType convertToProto(core::OrderType ordType) {
    switch (ordType) {
    case core::OrderType::limit:
        return transport::OrderType::ORDER_TYPE_LIMIT;
    case core::OrderType::market:
        return transport::OrderType::ORDER_TYPE_MARKET;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

inline transport::TimeInForce convertToProto(core::TimeInForce tif) {
    switch (tif) {
    case core::TimeInForce::day:
        return transport::TimeInForce::TIF_DAY;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

inline transport::ExecTransType convertToProto(core::ExecTransType execTransType) {
    switch (execTransType) {
    case core::ExecTransType::exectrans_new:
        return transport::ExecTransType::EXEC_TRANS_NEW;
    case core::ExecTransType::exectrans_cancel:
        return transport::ExecTransType::EXEC_TRANS_CANCEL;
    case core::ExecTransType::exectrans_correct:
        return transport::ExecTransType::EXEC_TRANS_CORRECT;
    case core::ExecTransType::exectrans_status:
        return transport::ExecTransType::EXEC_TRANS_STATUS;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

inline transport::ExecTypeOrOrderStatus
convertToProto(core::ExecTypeOrOrderStatus execTypeOrOrderStatus) {
    switch (execTypeOrOrderStatus) {
    case core::ExecTypeOrOrderStatus::status_new:
        return transport::ExecTypeOrOrderStatus::STATUS_NEW;
    case core::ExecTypeOrOrderStatus::status_partiallyFilled:
        return transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED;
    case core::ExecTypeOrOrderStatus::status_filled:
        return transport::ExecTypeOrOrderStatus::STATUS_FILLED;
    case core::ExecTypeOrOrderStatus::status_canceled:
        return transport::ExecTypeOrOrderStatus::STATUS_CANCELED;
    case core::ExecTypeOrOrderStatus::status_pendingCancel:
        return transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL;
    case core::ExecTypeOrOrderStatus::status_rejected:
        return transport::ExecTypeOrOrderStatus::STATUS_REJECTED;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// From proto enum to core enum
inline core::Side convertToInternal(transport::Side side) {
    switch (side) {
    case transport::Side::SIDE_BID:
        return core::Side::bid;
    case transport::Side::SIDE_ASK:
        return core::Side::ask;
    default:
        throw std::invalid_argument("Unknown Side enum value");
    }
}

inline core::OrderType convertToInternal(transport::OrderType ordType) {
    switch (ordType) {
    case transport::OrderType::ORDER_TYPE_LIMIT:
        return core::OrderType::limit;
    case transport::OrderType::ORDER_TYPE_MARKET:
        return core::OrderType::market;
    default:
        throw std::invalid_argument("Unknown OrderType enum value");
    }
}

inline core::TimeInForce convertToInternal(transport::TimeInForce tif) {
    switch (tif) {
    case transport::TimeInForce::TIF_DAY:
        return core::TimeInForce::day;
    default:
        throw std::invalid_argument("Unknown TimeInForce enum value");
    }
}

inline core::ExecTransType convertToInternal(transport::ExecTransType execTransType) {
    switch (execTransType) {
    case transport::ExecTransType::EXEC_TRANS_NEW:
        return core::ExecTransType::exectrans_new;
    case transport::ExecTransType::EXEC_TRANS_CANCEL:
        return core::ExecTransType::exectrans_cancel;
    case transport::ExecTransType::EXEC_TRANS_CORRECT:
        return core::ExecTransType::exectrans_correct;
    case transport::ExecTransType::EXEC_TRANS_STATUS:
        return core::ExecTransType::exectrans_status;
    default:
        throw std::invalid_argument("Unknown ExecTransType enum value");
    }
}

inline core::ExecTypeOrOrderStatus
convertToInternal(transport::ExecTypeOrOrderStatus execTypeOrOrderStatus) {
    switch (execTypeOrOrderStatus) {
    case transport::ExecTypeOrOrderStatus::STATUS_NEW:
        return core::ExecTypeOrOrderStatus::status_new;
    case transport::ExecTypeOrOrderStatus::STATUS_PARTIALLY_FILLED:
        return core::ExecTypeOrOrderStatus::status_partiallyFilled;
    case transport::ExecTypeOrOrderStatus::STATUS_FILLED:
        return core::ExecTypeOrOrderStatus::status_filled;
    case transport::ExecTypeOrOrderStatus::STATUS_CANCELED:
        return core::ExecTypeOrOrderStatus::status_canceled;
    case transport::ExecTypeOrOrderStatus::STATUS_PENDING_CANCEL:
        return core::ExecTypeOrOrderStatus::status_pendingCancel;
    case transport::ExecTypeOrOrderStatus::STATUS_REJECTED:
        return core::ExecTypeOrOrderStatus::status_rejected;
    default:
        throw std::invalid_argument("Unknown ExecTypeOrOrderStatus enum value");
    }
}

// Serializer and deserializer functions
inline std::string serializeContainer(const core::NewOrderSingleContainer& container) {
    transport::NewOrderSingleContainer containerProto;

    containerProto.set_cl_ord_id(container.clOrdId);
    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    containerProto.set_order_qty(container.orderQty);
    containerProto.set_ord_type(convertToProto(container.ordType));
    // Don't serialize price if not set to save some space
    if (container.price.has_value()) {
        containerProto.set_price(container.price.value());
    }
    containerProto.set_time_in_force(convertToProto(container.timeInForce));

    return containerProto.SerializeAsString();
}

inline std::string serializeContainer(const core::CancelOrderRequestContainer& container) {
    transport::CancelOrderRequestContainer containerProto;

    containerProto.set_cl_ord_id(container.clOrdId);
    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_order_id(container.orderId);
    containerProto.set_orig_cl_ord_id(container.origClOrdId);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    containerProto.set_order_qty(container.orderQty);

    return containerProto.SerializeAsString();
}

// TODO: Who will actually create these execution report containers? Gateway? Order Manager?
inline std::string serializeContainer(const core::ExecutionReportContainer& container) {
    transport::ExecutionReportContainer containerProto;

    containerProto.set_sender_comp_id(container.senderCompId);
    containerProto.set_target_comp_id(container.targetCompId);
    containerProto.set_order_id(container.orderId);
    containerProto.set_cl_order_id(container.clOrderId);
    if (container.origClOrdID.has_value()) {
        containerProto.set_orig_cl_ord_id(container.origClOrdID.value());
    }
    containerProto.set_exec_id(container.execId);
    containerProto.set_exec_trans_type(convertToProto(container.execTransType));
    containerProto.set_exec_type(convertToProto(container.execType));
    containerProto.set_ord_status(convertToProto(container.ordStatus));
    containerProto.set_ord_reject_reason(container.ordRejectReason);
    containerProto.set_symbol(container.symbol);
    containerProto.set_side(convertToProto(container.side));
    if (container.price.has_value()) {
        containerProto.set_price(container.price.value());
    }
    if (container.timeInForce.has_value()) {
        containerProto.set_time_in_force(convertToProto(container.timeInForce.value()));
    }
    containerProto.set_leaves_qty(container.leavesQty);
    containerProto.set_cum_qty(container.cumQty);
    containerProto.set_avg_px(container.avgPx);

    return containerProto.SerializeAsString();
}

inline core::NewOrderSingleContainer deserializeContainer(const std::string& data) {
    // switch on cases to find out whether data is NewOrderSingleContainer or
    // CancelOrderRequestContainer or ExecutionReportContainer

    throw std::runtime_error("unimplemented");
}

} // namespace transport

#endif // TRANSPORT_MESSAGING_H
