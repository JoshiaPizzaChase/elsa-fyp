#ifndef TRANSPORT_WEBSOCKET_CLIENT_H
#define TRANSPORT_WEBSOCKET_CLIENT_H

#include <boost/asio/placeholders.hpp>
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
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/roles/client_endpoint.hpp>
#include <websocketpp/roles/server_endpoint.hpp>
#include <websocketpp/server.hpp>

namespace transport {

// TODO: Why pass handle by value then move? Why not const ref?
// TODO: I need a "server connection meta data" class too then...  or make the onOpen generic?
using Client = websocketpp::client<websocketpp::config::asio_client>;

class ClientConnectionMetadata {
  public:
    using connMetaSharedPtr = websocketpp::lib::shared_ptr<ClientConnectionMetadata>;

    ClientConnectionMetadata(int id, websocketpp::connection_hdl handle, std::string_view uri)
        : m_id{id}, m_handle{std::move(handle)}, m_status{"Connecting"}, m_uri{uri},
          m_server{"N/A"} {
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

    friend std::ostream& operator<<(std::ostream& out, ClientConnectionMetadata const& data) {
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
        // Set logging settings here
    }

    ~OrderManagerClient() {
        m_endpoint.stop_perpetual();

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

        // TODO: Use thread guards instead of manual joins!
        if (m_thread->joinable()) {
            m_thread->join();
        }
    }

    /*
     * Initialises asio config on client endpoint.
     * Perpetual mode means endpoint won't exit processing loop even when there are no connections.
     * Launches new thread to handle network processing.
     */
    void start() {
        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        // Launches a new thread
        m_thread = std::make_shared<websocketpp::lib::thread>(&Client::run, &m_endpoint);
    }

    // TODO: Replace error code return value with std::expected!!!
    int connect(std::string const& uri) {
        websocketpp::lib::error_code errorCode;

        Client::connection_ptr connection = m_endpoint.get_connection(uri, errorCode);

        if (errorCode) {
            std::println("Connect initialization error: {}", errorCode.message());
            return -1;
        }

        int new_id = m_nextId++;
        ClientConnectionMetadata::connMetaSharedPtr metadata_ptr(
            new ClientConnectionMetadata(new_id, connection->get_handle(), uri));
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

    void send(int id, const std::string& message) {
        websocketpp::lib::error_code errorCode;

        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            std::println("No connection found with id {}", id);
            return;
        }

        m_endpoint.send(metadata_it->second->getHandle(), static_cast<std::string>(message),
                        websocketpp::frame::opcode::binary, errorCode);
        if (errorCode) {
            std::println("Error sending message: {}", errorCode.message());

            return;
        }

        metadata_it->second->recordSentMessage(message);
    }

    ClientConnectionMetadata::connMetaSharedPtr getMetadata(int id) const {
        auto metadata_it = m_connectionMap.find(id);
        if (metadata_it == m_connectionMap.end()) {
            return {};
        }

        return metadata_it->second;
    }

  private:
    using ConnectionMap = std::unordered_map<int, ClientConnectionMetadata::connMetaSharedPtr>;
    Client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    ConnectionMap m_connectionMap;
    int m_nextId{0};
};

} // namespace transport

#endif
