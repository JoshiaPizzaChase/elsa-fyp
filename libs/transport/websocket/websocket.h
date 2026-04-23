#pragma once

#include "config.h"
#include "core/thread_safe_queue.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "transport/message_format.h"
#include <concepts>
#include <expected>
#include <mutex>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <websocketpp/common/connection_hdl.hpp>
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
    closing,
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
        if (connection->is_server()) {
            m_counter_party = connection->get_request_header("client_name");
        } else {
            m_counter_party = connection->get_response_header("Server");
        }
    }

    void on_fail(Endpoint* endpoint, websocketpp::connection_hdl handle) {
        m_status = ConnectionStatus::failed;

        typename Endpoint::connection_ptr connection = endpoint->get_con_from_hdl(handle);
        if (connection->is_server()) {
            m_counter_party = connection->get_request_header("client_name");
        } else {
            m_counter_party = connection->get_response_header("Server");
        }
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

    websocketpp::connection_hdl get_handle() const {
        return m_handle;
    }

    int get_id() const {
        return m_id;
    }

    std::string get_uri() const {
        return m_uri;
    }

    std::string get_counter_party() const {
        return m_counter_party;
    }

    ConnectionStatus get_status() const {
        return m_status;
    }

    void set_status(ConnectionStatus status) {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_status = status;
    }

    std::string get_error_reason() const {
        return m_error_reason;
    }

    // Makes a copy of the message store and returns it!
    // This is expensive, use cautiously.
    std::vector<std::string> get_message_store() const {
        return m_message_store;
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
            << "> Counter party: " << data.m_counter_party << "\n"
            << "> Error/close reason: "
            << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason);

        out << "> Messages Processed: (" << data.m_message_store.size() << ") \n";

        for (const auto& message : data.m_message_store) {
            out << message << "\n";
        }

        return out;
    }

  private:
    std::mutex m_mtx;
    int m_id;
    websocketpp::connection_hdl m_handle;
    ConnectionStatus m_status;
    std::string m_uri;
    std::string m_counter_party;
    std::string m_error_reason;
    std::vector<std::string> m_message_store;
    core::ThreadSafeQueue<std::string> m_message_queue;
};

template <ClientOrServer Endpoint>
class WebsocketManager {
  protected:
    using ConnectionHandle = websocketpp::connection_hdl;

  public:
    using ConnectionMetadata = ConnectionMetadata<Endpoint>;
    // Constructor if you already have a logger, usually when a top-level class owns
    // a WebsocketManager object.
    WebsocketManager(std::shared_ptr<spdlog::logger> logger) : m_logger{logger} {
        init_logging(logger->name());
    }

    // Constructor for stand-alone WebsocketManager objects.
    WebsocketManager(std::string_view logger_name)
        : m_logger{spdlog::basic_logger_mt<spdlog::async_factory>(
              static_cast<std::string>(logger_name),
              std::string(PROJECT_SOURCE_DIR) + std::format("/logs/{}.log", logger_name))} {
        init_logging(logger_name);
    }

    virtual ~WebsocketManager() {
        if (!close_all().has_value()) {
            m_logger->error("Failed to close some ids during destruction.");
        }

        if (m_thread && m_thread->joinable()) {
            m_thread->join();
        }
    }

    virtual std::expected<void, int> start() = 0;
    virtual std::expected<void, int> stop() = 0;

    std::expected<void, int> send(int id, const std::string& message,
                                  MessageFormat message_format = MessageFormat::binary) {
        websocketpp::lib::error_code error_code;

        auto metadata_it = m_id_to_connection_map.find(id);
        if (metadata_it == m_id_to_connection_map.end()) {
            m_logger->error("Sending failed, no connection found with id: {}", id);
            return std::unexpected{-1};
        }
        const auto opcode{message_format == MessageFormat::text
                              ? websocketpp::frame::opcode::text
                              : websocketpp::frame::opcode::binary};
        m_endpoint.send(metadata_it->second->get_handle(), message, opcode, error_code);
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
            } else {
                it.second->set_status(ConnectionStatus::closing);
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

    // Useful if you want to iterate over all connections in the map.
    // For example, getting the list of connection names.
    // Since it returns a const reference, use cautiously to avoid dangling references.
    const std::unordered_map<int, typename ConnectionMetadata::conn_meta_shared_ptr>&
    get_id_to_connection_map() const {
        return m_id_to_connection_map;
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
        std::ostream* log_stream = new std::ofstream(std::format(
            "{}/logs/{}/{}_endpoint.log", PROJECT_SOURCE_DIR, SERVER_NAME, logger_name));
        m_endpoint.get_alog().set_ostream(log_stream);
        m_endpoint.get_elog().set_ostream(log_stream);
    }
};

} // namespace transport
