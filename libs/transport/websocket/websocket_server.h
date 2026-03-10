#pragma once

#include "websocket.h"
#include <boost/asio/placeholders.hpp>
#include <expected>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/frame.hpp>

namespace transport {

class WebsocketManagerServer : public WebsocketManager<Server> {

  public:
    WebsocketManagerServer(int port, std::string_view uri, std::shared_ptr<spdlog::logger> logger,
                           bool reuse_addr = true)
        : m_port{port}, WebsocketManager{logger} {
        init_handlers(uri, reuse_addr);
        m_logger->info("Websocket server initialized on port {}, uri {}", m_port, uri);
    }

    WebsocketManagerServer(int port, std::string_view uri,
                           std::string_view logger_name = "server_websocket_logger",
                           bool reuse_addr = true)
        : m_port{port}, WebsocketManager{logger_name} {
        init_handlers(uri, reuse_addr);
        m_logger->info("Websocket server initialized on port {}, uri {}", m_port, uri);
    }

    ~WebsocketManagerServer() {
        websocketpp::lib::error_code error_code;
        m_endpoint.stop_listening(error_code);

        if (error_code) {
            m_logger->error(
                "Websocket manager server failed to stop listening during destruction: {}",
                error_code.message());
        }
    }

    /*
     * Initialises asio config on server endpoint.
     * Starts listening to connections on port.
     * Launches new thread to handle network processing.
     */
    std::expected<void, int> start() override {
        m_endpoint.init_asio();

        m_endpoint.listen(m_port);

        websocketpp::lib::error_code error_code;
        m_endpoint.start_accept(error_code);

        if (error_code) {
            m_logger->error("Error starting server: {}", error_code.message());
            return std::unexpected{-1};
        }

        m_thread = std::make_shared<websocketpp::lib::thread>(&Server::run, &m_endpoint);
        return {};
    }

    /*
     * Stops websocket server gracefully.
     */
    std::expected<void, int> stop() override {

        websocketpp::lib::error_code error_code;
        m_endpoint.stop_listening(error_code);

        if (error_code) {
            m_logger->error(
                "Websocket manager server failed to stop listening during manual stop: {}",
                error_code.message());
            return std::unexpected{-1};
        }

        if (!close_all().has_value()) {
            m_logger->error("Websocket manager server failed to close some connections.");
            return std::unexpected{-1};
        }

        return {};
    }

    /*
     * Server-side specific command to send the same message to all connections. If some of the
     * connections fail to send, broadcasting does not halt. It returns a vector of those failed
     * ids.
     */
    std::expected<void, std::vector<int>> send_to_all(const std::string& message,
                                                      MessageFormat message_format) {
        m_logger->info("Broadcasting...");
        m_logger->flush();
        std::vector<int> failed_ids;
        for (const auto& it : m_id_to_connection_map) {
            if (!send(it.first, message, message_format)) {
                failed_ids.push_back(it.first);
            }
        }

        if (!failed_ids.empty()) {
            m_logger->error("Failed to send message to some ids.");
            m_logger->flush();
            return std::unexpected(failed_ids);
        }

        return {};
    }

  private:
    using handle_to_connection_map =
        std::map<ConnectionHandle, ConnectionMetadata::conn_meta_shared_ptr,
                 std::owner_less<ConnectionHandle>>;
    int m_port;
    handle_to_connection_map m_handle_to_connection_map;

    void init_handlers(std::string_view uri, bool reuse_addr) {
        m_endpoint.set_reuse_addr(reuse_addr);

        m_endpoint.set_open_handler([this, uri](ConnectionHandle handle) {
            int new_id = m_next_id++;
            ConnectionMetadata::conn_meta_shared_ptr metadata_ptr{
                std::make_shared<ConnectionMetadata>(new_id, handle, uri)};

            metadata_ptr->on_open(&m_endpoint, handle);
            m_id_to_connection_map.emplace(new_id, metadata_ptr);
            m_handle_to_connection_map.emplace(handle, std::move(metadata_ptr));
        });
        m_endpoint.set_message_handler([this](ConnectionHandle handle, Server::message_ptr msg) {
            if (auto it{m_handle_to_connection_map.find(handle)};
                it != m_handle_to_connection_map.end()) {
                it->second->on_message(handle, msg);
            }
        });
        m_endpoint.set_close_handler([this](ConnectionHandle handle) {
            if (auto it{m_handle_to_connection_map.find(handle)};
                it != m_handle_to_connection_map.end()) {
                it->second->on_close(&m_endpoint, handle);
            }
        });
        m_endpoint.set_fail_handler([this](ConnectionHandle handle) {
            if (auto it{m_handle_to_connection_map.find(handle)};
                it != m_handle_to_connection_map.end()) {
                it->second->on_fail(&m_endpoint, handle);
            }
        });
        m_endpoint.set_ping_handler([this](ConnectionHandle handle, std::string str) {
            m_logger->info("Received ping from connection.");
            m_endpoint.pong(handle, str);
            return true;
        });
    }
};

} // namespace transport
