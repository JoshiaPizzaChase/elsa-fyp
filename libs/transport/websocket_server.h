#ifndef TRANSPORT_WEBSOCKET_SERVER_H
#define TRANSPORT_WEBSOCKET_SERVER_H

#include "websocket.h"
#include <boost/asio/placeholders.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/roles/server_endpoint.hpp>
#include <websocketpp/server.hpp>

namespace transport {

using Server = websocketpp::server<websocketpp::config::asio>;

class OrderManagerServer : public WebsocketManager<Server> {
  public:
    OrderManagerServer(int port) : m_port{port} {
        // Set logging settings here

        m_endpoint.set_open_handler([this](ConnectionHandle handle) {
            int newId = m_nextId++;
            ConnectionMetadata::connMetaSharedPtr metadataPtr{
            // TODO: Get uri from env/config files
                std::make_shared<ConnectionMetadata>(newId, handle, "localhost")};

            m_idToConnectionMap.emplace(newId, metadataPtr);
            m_handleToConnectionMap.emplace(handle, std::move(metadataPtr));
        });

        m_endpoint.set_message_handler([this](ConnectionHandle handle, Server::message_ptr msg) {
            if (auto it{m_handleToConnectionMap.find(handle)};
                it != m_handleToConnectionMap.end()) {
                it->second->onMessage(handle, msg);
            }
        });
    }

    ~OrderManagerServer() {
    }

    /*
     * Initialises asio config on server endpoint.
     * Starts listening to connections on port.
     * Launches new thread to handle network processing.
     * Returns false if failed to start.
     */
    // TODO: Replace error code return value with std::expected!!!
    std::expected<void, int> start() override {
        m_endpoint.init_asio();

        m_endpoint.listen(m_port);

        websocketpp::lib::error_code errorCode;
        m_endpoint.start_accept(errorCode);

        if (errorCode) {
            return std::unexpected{-1};
        }

        m_thread = std::make_shared<websocketpp::lib::thread>(&Server::run, &m_endpoint);
        return {};
    }

  private:
    using HandleToConnectionMap = std::map<ConnectionHandle, ConnectionMetadata::connMetaSharedPtr,
                                           std::owner_less<ConnectionHandle>>;
    int m_port;
    HandleToConnectionMap m_handleToConnectionMap;
};

} // namespace transport

#endif
