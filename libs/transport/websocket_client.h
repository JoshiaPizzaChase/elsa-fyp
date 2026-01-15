#ifndef TRANSPORT_WEBSOCKET_CLIENT_H
#define TRANSPORT_WEBSOCKET_CLIENT_H

#include "websocket.h"
#include <boost/asio/placeholders.hpp>
#include <memory>
#include <print>
#include <websocketpp/client.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/roles/client_endpoint.hpp>
#include <websocketpp/roles/server_endpoint.hpp>
#include <websocketpp/server.hpp>

namespace transport {

using Client = websocketpp::client<websocketpp::config::asio_client>;

class WebsocketManagerClient : public WebsocketManager<Client> {
  public:
    WebsocketManagerClient() {
        // Set logging settings here
    }

    ~WebsocketManagerClient() {
        m_endpoint.stop_perpetual();
        // Do I need to call parent class destructor here to close connections?
    }

    /*
     * Initialises asio config on client endpoint.
     * Perpetual mode means endpoint won't exit processing loop even when there are no connections.
     * Launches new thread to handle network processing.
     */
    std::expected<void, int> start() override {
        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        // Launches a new thread
        m_thread = std::make_shared<websocketpp::lib::thread>(&Client::run, &m_endpoint);
        return {};
    }

    // TODO: Replace error code return value with std::expected!!!
    std::expected<int, int> connect(std::string const& uri) {
        websocketpp::lib::error_code errorCode;

        Client::connection_ptr connection = m_endpoint.get_connection(uri, errorCode);

        if (errorCode) {
            std::println("Connect initialization error: {}", errorCode.message());
            return std::unexpected{-1};
        }

        int new_id = m_nextId++;

        ConnectionMetadata::connMetaSharedPtr metadata_ptr{
            std::make_shared<ConnectionMetadata>(new_id, connection->get_handle(), uri)};

        m_idToConnectionMap[new_id] = metadata_ptr;

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
};

} // namespace transport

#endif
