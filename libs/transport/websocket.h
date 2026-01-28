#ifndef TRANSPORT_WEBSOCKET_H
#define TRANSPORT_WEBSOCKET_H

#include <boost/asio/placeholders.hpp>
#include <expected>
#include <print>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <websocketpp/close.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/frame.hpp>

namespace transport {

// TODO: Impose a Concept here
template <typename Endpoint>
struct ConnectionMetadata {
  public:
    using connMetaSharedPtr = websocketpp::lib::shared_ptr<ConnectionMetadata>;

    ConnectionMetadata(int id, websocketpp::connection_hdl handle, std::string_view uri)
        : m_id{id}, m_handle{std::move(handle)}, m_status{"Connecting"}, m_uri{uri},
          m_counterParty{"N/A"} {
    }

    // Handler
    // TODO: Maybe convert to non-const reference instead of raw pointers... not sure why the
    // getters are non-const methods.
    void onOpen(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = "Open";

        typename Endpoint::connection_ptr connection =
            endpoint->get_con_from_hdl(std::move(handle));
        m_counterParty = connection->get_response_header("Server");
    }

    void onFail(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = "Failed";

        typename Endpoint::connection_ptr connection =
            endpoint->get_con_from_hdl(std::move(handle));
        m_counterParty = connection->get_response_header("Server");
        m_errorReason = connection->get_ec().message();
    }

    void onClose(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = "Closed";
        typename Endpoint::connection_ptr connection =
            endpoint->get_con_from_hdl(std::move(handle));
        std::stringstream s;
        s << "close code: " << connection->get_remote_close_code() << " ("
          << websocketpp::close::status::get_string(connection->get_remote_close_code())
          << "), close reason: " << connection->get_remote_close_reason();
        m_errorReason = s.str();
    }

    void onMessage(websocketpp::connection_hdl handle, Endpoint::message_ptr msg) {
        // TODO: handle different opcode formats?
        m_messageQueue.emplace(msg->get_payload());
    }

    websocketpp::connection_hdl getHandle() const {
        return m_handle;
    }

    int getId() const {
        return m_id;
    }

    std::string getStatus() const {
        return m_status;
    }

    std::optional<std::string> dequeueMessage() {
        if (m_messageQueue.empty()) {
            return std::nullopt;
        }
        const auto first{m_messageQueue.front()};
        m_messageQueue.pop();
        return first;
    }

    void recordSentMessage(std::string_view message) {
        m_messageStore.emplace_back(message);
    }

    friend std::ostream& operator<<(std::ostream& out, ConnectionMetadata const& data) {
        out << "> URI: " << data.m_uri << "\n"
            << "> Status: " << data.m_status << "\n"
            << "> Remote Server: "
            << (data.m_counterParty.empty() ? "None Specified" : data.m_counterParty) << "\n"
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
    // TODO: m_counterParty may either be server or client, how would we handle these cases
    // separately? We may have to change the config...
    std::string m_counterParty;
    std::string m_errorReason;
    std::vector<std::string> m_messageStore;
    std::queue<std::string> m_messageQueue;
};

// TODO: Impose a Concept here
template <typename Endpoint>
class WebsocketManager {
  protected:
    using ConnectionMetadata = ConnectionMetadata<Endpoint>;
    using ConnectionHandle = websocketpp::connection_hdl;

  public:
    WebsocketManager() = default;

    virtual ~WebsocketManager() {
        for (const auto& it : m_idToConnectionMap) {
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

    virtual std::expected<void, int> start() = 0;

    virtual std::expected<void, int> send(int id, const std::string& message) {
        websocketpp::lib::error_code errorCode;

        auto metadata_it = m_idToConnectionMap.find(id);
        if (metadata_it == m_idToConnectionMap.end()) {
            std::println("No connection found with id {}", id);
            return std::unexpected{-1};
        }

        m_endpoint.send(metadata_it->second->getHandle(), message,
                        websocketpp::frame::opcode::text, errorCode);
        if (errorCode) {
            std::println("Error sending message: {}", errorCode.message());
            return std::unexpected{-1};
        }

        metadata_it->second->recordSentMessage(message);
        return {};
    }

    virtual std::expected<void, int> close(int id, websocketpp::close::status::value code) {
        websocketpp::lib::error_code errorCode;

        auto metadata_it = m_idToConnectionMap.find(id);
        if (metadata_it == m_idToConnectionMap.end()) {
            std::println("No connection found with id {}", id);
            return std::unexpected{-1};
        }

        m_endpoint.close(metadata_it->second->getHandle(), code, "", errorCode);
        if (errorCode) {
            std::println("Error initiating close: {}", errorCode.message());
            return std::unexpected{-1};
        }

        return {};
    }

    // TODO: Who will do round robining?
    // TODO: Who will deserialize?
    virtual std::optional<std::string> dequeueMessage(int id) {
        auto it{m_idToConnectionMap.find(id)};

        if (it == m_idToConnectionMap.end()) {
            return std::nullopt;
        }

        return it->second->dequeueMessage();
    }

    ConnectionMetadata::connMetaSharedPtr getMetadata(int id) const {
        auto metadata_it = m_idToConnectionMap.find(id);
        if (metadata_it == m_idToConnectionMap.end()) {
            return {};
        }

        return metadata_it->second;
    }

  protected:
    using IdToConnectionMap =
        std::unordered_map<int, typename ConnectionMetadata::connMetaSharedPtr>;
    Endpoint m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    IdToConnectionMap m_idToConnectionMap;
    int m_nextId{0};
};

} // namespace transport

#endif // TRANSPORT_WEBSOCKET_H
