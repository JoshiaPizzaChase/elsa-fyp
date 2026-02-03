#ifndef TRANSPORT_WEBSOCKET_H
#define TRANSPORT_WEBSOCKET_H

#include "config.h"
#include "core/thread_safe_queue.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <boost/asio/placeholders.hpp>
#include <concepts>
#include <expected>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/server.hpp>

namespace transport {

using Client = websocketpp::client<websocketpp::config::asio_client>;
using Server = websocketpp::server<websocketpp::config::asio>;

template <typename T>
concept ClientOrServer = std::same_as<T, Client> || std::same_as<T, Server>;

enum class ConnectionStatus {
    connecting,
    open,
    failed,
    closed,
};

template <ClientOrServer Endpoint>
struct ConnectionMetadata {
  public:
    using conn_meta_shared_ptr = websocketpp::lib::shared_ptr<ConnectionMetadata>;

    ConnectionMetadata(int id, websocketpp::connection_hdl handle, std::string_view uri)
        : m_id{id}, m_handle{handle}, m_status{ConnectionStatus::connecting}, m_uri{uri},
          m_counter_party{"N/A"} {
    }

    // Handlers
    void on_open(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = ConnectionStatus::open;

        typename Endpoint::connection_ptr connection = endpoint->get_con_from_hdl(handle);
        m_counter_party = connection->get_response_header("Server");
    }

    void on_fail(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = ConnectionStatus::failed;

        typename Endpoint::connection_ptr connection = endpoint->get_con_from_hdl(handle);
        m_counter_party = connection->get_response_header("Server");
        m_error_reason = connection->get_ec().message();
    }

    void on_close(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = ConnectionStatus::closed;

        typename Endpoint::connection_ptr connection = endpoint->get_con_from_hdl(handle);
        m_error_reason = std::format(
            "Close code: {} ({}), close reason: {}", connection->get_remote_close_code(),
            websocketpp::close::status::get_string(connection->get_remote_close_code()),
            connection->get_remote_close_reason());
    }

    void on_message(websocketpp::connection_hdl handle, Endpoint::message_ptr msg) {
        // TODO: handle different opcode formats more efficiently?
        m_message_queue.enqueue(std::move(msg->get_payload()));
    }

    // Getters
    websocketpp::connection_hdl get_handle() const {
        return m_handle;
    }

    int get_id() const {
        return m_id;
    }

    ConnectionStatus get_status() const {
        return m_status;
    }

    // Message queuing and storing
    std::optional<std::string> dequeue_message() {
        return m_message_queue.dequeue();
    }

    std::string wait_and_dequeue_message() {
        return m_message_queue.wait_and_dequeue();
    }

    void record_sent_message(std::string_view message) {
        m_message_store.emplace_back(message);
    }

    friend std::ostream& operator<<(std::ostream& out, ConnectionMetadata const& data) {
        out << "> URI: " << data.m_uri << "\n"
            << "> Status: " << static_cast<int>(data.m_status) << "\n"
            << (data.m_counter_party.empty() ? "None Specified" : data.m_counter_party) << "\n"
            << "> Error/close reason: "
            << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason);

        out << "> Messages Processed: (" << data.m_message_store.size() << ") \n";

        for (const auto& message : data.m_message_store) {
            out << message << "\n";
        }

        return out;
    }

  private:
    int m_id;
    websocketpp::connection_hdl m_handle;
    ConnectionStatus m_status;
    std::string m_uri;
    // TODO: m_counter_party may either be server or client, how would we handle these cases
    // separately? We may have to change the config...
    std::string m_counter_party;
    std::string m_error_reason;
    std::vector<std::string> m_message_store;
    core::ThreadSafeQueue<std::string> m_message_queue;
};

template <ClientOrServer Endpoint>
class WebsocketManager {
  protected:
    using ConnectionMetadata = ConnectionMetadata<Endpoint>;
    using ConnectionHandle = websocketpp::connection_hdl;

  public:
    // Constructor if you already have a logger, usually when a top-level class owns
    // a WebsocketManager object.
    WebsocketManager(std::shared_ptr<spdlog::logger> logger) : m_logger{logger} {
        init_logging(logger->name());
    }

    // Constructor for stand-alone WebsocketManager objects.
    WebsocketManager(std::string_view logger_name)
        : m_logger{spdlog::basic_logger_mt<spdlog::async_factory>(
              static_cast<std::string>(logger_name),
              std::string(PROJECT_SOURCE_DIR) + std::format("logs/{}.log", logger_name))} {
        init_logging(logger_name);
    }

    virtual ~WebsocketManager() {
        if (!close_all().has_value()) {
            m_logger->error("Failed to close some ids during destruction.");
        }

        if (m_thread->joinable()) {
            m_thread->join();
        }
    }

    virtual std::expected<void, int> start() = 0;
    virtual std::expected<void, int> stop() = 0;

    std::expected<void, int> send(int id, const std::string& message) {
        websocketpp::lib::error_code error_code;

        auto metadata_it = m_id_to_connection_map.find(id);
        if (metadata_it == m_id_to_connection_map.end()) {
            m_logger->error("Sending failed, no connection found with id: {}", id);
            return std::unexpected{-1};
        }

        m_endpoint.send(metadata_it->second->get_handle(), message,
                        websocketpp::frame::opcode::text, error_code);
        if (error_code) {
            m_logger->error("Error sending message: {}", error_code.message());
            return std::unexpected{-1};
        }

        metadata_it->second->record_sent_message(message);
        return {};
    }

    std::expected<void, int> close(int id, websocketpp::close::status::value code) {
        websocketpp::lib::error_code error_code;

        auto metadata_it = m_id_to_connection_map.find(id);

        if (metadata_it == m_id_to_connection_map.end()) {
            m_logger->error("No connection found with id: {}", id);
            return std::unexpected{-1};
        }

        m_endpoint.close(metadata_it->second->get_handle(), code, "", error_code);
        if (error_code) {
            m_logger->error("Error initiating close: {}", error_code.message());
            return std::unexpected{-1};
        }

        return {};
    }

    std::expected<void, std::vector<int>> close_all() {
        m_logger->info("Closing all connections...");

        std::vector<int> failed_ids;
        for (const auto& it : m_id_to_connection_map) {
            const auto id{it.second->get_id()};
            if (it.second->get_status() != ConnectionStatus::open) {
                m_logger->info("Connection id {} has status {}, skipping.", id,
                               static_cast<int>(it.second->get_status()));
                continue;
            }

            m_logger->info("Closing connection: {} with status {}", id,
                           static_cast<int>(it.second->get_status()));

            websocketpp::lib::error_code error_code;
            m_endpoint.close(it.second->get_handle(), websocketpp::close::status::going_away, "",
                             error_code);
            if (error_code) {
                m_logger->error("Error closing connection {}: {}", id, error_code.message());
                failed_ids.push_back(id);
            }
        }

        if (!failed_ids.empty()) {
            return std::unexpected{failed_ids};
        }

        return {};
    }

    std::optional<std::string> dequeue_message(int id) {
        auto it{m_id_to_connection_map.find(id)};

        if (it == m_id_to_connection_map.end()) {
            return std::nullopt;
        }

        return it->second->dequeue_message();
    }

    // THIS IS A BLOCKING DEQUEUE METHOD.
    // It sleeps the thread if queue is non-emoty, do not use this if you want busy-waiting!
    // Returns a null optional if no id is found.
    std::optional<std::string> wait_and_dequeue_message(int id) {
        auto it{m_id_to_connection_map.find(id)};

        if (it == m_id_to_connection_map.end()) {
            return std::nullopt;
        }

        return it->second->wait_and_dequeue_message();
    }

    ConnectionMetadata::conn_meta_shared_ptr get_metadata(int id) const {
        auto metadata_it = m_id_to_connection_map.find(id);
        if (metadata_it == m_id_to_connection_map.end()) {
            return {};
        }

        return metadata_it->second;
    }

  protected:
    using IdToConnectionMap =
        std::unordered_map<int, typename ConnectionMetadata::conn_meta_shared_ptr>;
    Endpoint m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    IdToConnectionMap m_id_to_connection_map;
    int m_next_id{0};
    std::shared_ptr<spdlog::logger> m_logger;

    void init_logging(std::string_view logger_name) {
        // Intensive logging
        m_endpoint.set_access_channels(websocketpp::log::alevel::all);
        m_endpoint.set_error_channels(websocketpp::log::elevel::all);

        // Redirect endpoint logs to separate file from spdlogs
        std::ostream* log_stream = new std::ofstream(
            PROJECT_SOURCE_DIR + std::format("/logs/{}_endpoint.log", logger_name));
        m_endpoint.get_alog().set_ostream(log_stream);
        m_endpoint.get_elog().set_ostream(log_stream);
    }
};

} // namespace transport

#endif // TRANSPORT_WEBSOCKET_H
