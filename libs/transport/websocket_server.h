#ifndef TRANSPORT_WEBSOCKET_SERVER_H
#define TRANSPORT_WEBSOCKET_SERVER_H

#include "containers.h"
#include <boost/asio/placeholders.hpp>
#include <memory>
#include <print>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/roles/server_endpoint.hpp>
#include <websocketpp/server.hpp>

namespace transport {

// TODO: Why pass handle by value then move? Why not const ref?
// TODO: I need a "server connection meta data" class too then...  or make the onOpen generic?
using Server = websocketpp::server<websocketpp::config::asio>;

class ServerConnectionMetadata {
  public:
    using connMetaSharedPtr = websocketpp::lib::shared_ptr<ServerConnectionMetadata>;

    ServerConnectionMetadata(int id, websocketpp::connection_hdl handle, std::string_view uri)
        : m_id{id}, m_handle{std::move(handle)}, m_status{"Connecting"}, m_uri{uri},
          m_client{"N/A"} {
    }

    // Handlers
    // TODO: Maybe convert to non-const reference instead of raw pointers... not sure why the
    // getters are non-const methods.
    void onOpen(Server* server, websocketpp::connection_hdl handle) {
        m_status = "Open";

        Server::connection_ptr connection = server->get_con_from_hdl(std::move(handle));
        m_client = connection->get_response_header("Server");
    }

    void onFail(Server* server, websocketpp::connection_hdl handle) {
        m_status = "Failed";

        Server::connection_ptr connection = server->get_con_from_hdl(std::move(handle));
        m_client = connection->get_response_header("Server");
        m_errorReason = connection->get_ec().message();
    }

    void onClose(Server* server, websocketpp::connection_hdl handle) {
        m_status = "Closed";
        Server::connection_ptr connection = server->get_con_from_hdl(std::move(handle));
        std::stringstream s;
        s << "close code: " << connection->get_remote_close_code() << " ("
          << websocketpp::close::status::get_string(connection->get_remote_close_code())
          << "), close reason: " << connection->get_remote_close_reason();
        m_errorReason = s.str();
    }

    void onMessage(websocketpp::connection_hdl handle, Server::message_ptr msg) {
        // TODO: handle different opcode formats?
        m_messageQueue.emplace(msg->get_payload());
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
    
    std::string dequeueMessage() {
        const auto first{m_messageQueue.back()};
        m_messageQueue.pop();
        return first;
    }

    void recordSentMessage(std::string_view message) {
        m_messageStore.emplace_back(message);
    }

    friend std::ostream& operator<<(std::ostream& out, ServerConnectionMetadata const& data) {
        out << "> URI: " << data.m_uri << "\n"
            << "> Status: " << data.m_status << "\n"
            << "> Remote Server: " << (data.m_client.empty() ? "None Specified" : data.m_client)
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
    std::string m_client;
    std::string m_errorReason;
    std::vector<std::string> m_messageStore;
    std::queue<std::string> m_messageQueue; // Uses a deque by default
};

class OrderManagerServer {
    // TODO: complete this class. To be used by order manager.
  public:
    OrderManagerServer(int port) : m_port{port} {
        // Set logging settings!
        // Set handlers! open handler, close handler, fail handler
        // Open handler: (handle) => { ConnectionMap.add(m_nextId++, new ConnectionMetadata(handle, ...))}
        // Close handler: remove...
        // Fail handler: ???
    }

    ~OrderManagerServer() {
        // Close all connections
        for (const auto& it : m_connectionMap) {
            if (it.second->getStatus() != "Open") {
                // Only close open connections. Lol but what happens if we don't?
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

        // TODO: Use thread guard!
        if (m_thread->joinable()) {
            m_thread->join();
        }
    }

    /*
     * Initialises asio config on server endpoint.
     * Starts listening to connections on port.
     * Launches new thread to handle network processing.
     * Returns false if failed to start.
     */
    // TODO: Replace error code return value with std::expected!!!
    [[nodiscard]] bool start() {
        m_endpoint.init_asio();

        m_endpoint.listen(m_port);

        websocketpp::lib::error_code errorCode;
        m_endpoint.start_accept(errorCode);

        if (errorCode) {
            return false;
        }

        m_thread = std::make_shared<websocketpp::lib::thread>(&Server::run, &m_endpoint);
        return true;
    }

    // member functions
    // 1. get server connection metadata
    ServerConnectionMetadata::connMetaSharedPtr getMetadata(int id) const {
        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            return {};
        }

        return metadata_it->second;
    }

    bool send(int id, std::string_view message) {
        throw std::runtime_error("unimplemented");
    }

    void close(int id, websocketpp::close::status::value code) {
        throw std::runtime_error("unimplemented");
    }

    // TODO: Who will do round robining?
    std::string dequeueMessage(int id) {
        // return m_connectionMap.lookup(id) -> dequeueMessage();
        throw std::runtime_error("unimplemented");
    }
  private:
    // member vars
    // 1. connection pool (one for each gateway connection), a mapping from id to connection wrapper
    // which contains handle
    using ConnectionMap = std::unordered_map<int, ServerConnectionMetadata::connMetaSharedPtr>;
    ConnectionMap m_connectionMap;
    // 2. server endpoint
    Server m_endpoint;
    // 3. next id for each new connection (we expect only gateway connections tbh?)
    int m_nextId{0};
    // 4. thread to handle network processing
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    // 5. port to listen on!
    int m_port;
};

} // namespace transport

#endif
