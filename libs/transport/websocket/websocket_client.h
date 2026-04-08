#pragma once

#include "websocket.h"
#include <boost/asio/placeholders.hpp>
#include <memory>
#include <websocketpp/client.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/frame.hpp>

namespace transport {

class WebsocketManagerClient : public WebsocketManager<Client> {
  public:
    WebsocketManagerClient(std::shared_ptr<spdlog::logger> logger) : WebsocketManager{logger} {
    }

    WebsocketManagerClient(std::string_view logger_name = "client_websocket_logger")
        : WebsocketManager{logger_name} {
    }

    ~WebsocketManagerClient() {
        if (!m_perpetual_active) {
            m_endpoint.stop_perpetual();
            m_perpetual_active = false; // Not necessary, only done for completeness.
        }
    }

    /*
     * Initialises asio config on client endpoint.
     * Perpetual mode means endpoint won't exit processing loop even when there are no connections.
     * Launches new thread to handle network processing.
     */
    std::expected<void, int> start() override {
        if (m_perpetual_active) {
            m_logger->info("Websocket manager client has already been started!");
            return std::unexpected{-1};
        }

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();
        m_perpetual_active = true;

        // Launches a new thread
        m_thread = std::make_shared<websocketpp::lib::thread>(&Client::run, &m_endpoint);
        return {};
    }

    /*
     * Stops websocket client gracefully.
     */
    std::expected<void, int> stop() override {
        m_logger->info("Manually stopping websocket manager client...");

        if (!m_perpetual_active) {
           m_logger->info("Websocket manager client has already been stopped!");
           return std::unexpected{-1};
        }

        m_endpoint.stop_perpetual();
        m_perpetual_active = false;

        if (!close_all().has_value()) {
            m_logger->error("Websocket manager client failed to close some connections.");
            return std::unexpected{-1};
        }
        m_logger->info("Websocket manager client closed all connections successfully.");

        return {};
    }

    /*
     * Connects to a server endpoint based on uri.
     * An example of uri is ws://localhost:6767.
     * Returns the connection id if successful.
     */
    std::expected<int, int> connect(std::string_view uri, std::string_view name = "N/A") {
        websocketpp::lib::error_code error_code;

        Client::connection_ptr connection =
            m_endpoint.get_connection(static_cast<std::string>(uri), error_code);

        if (error_code) {
            m_logger->error("Connect initialization error: {}", error_code.message());
            return std::unexpected{-1};
        }

        // We use a user-provided label, to help the server side identify the client.
        connection->append_header("client_name", static_cast<std::string>(name));

        int new_id = m_next_id++;

        ConnectionMetadata::conn_meta_shared_ptr metadata_ptr{std::make_shared<ConnectionMetadata>(
            new_id, connection->get_handle(), static_cast<std::string>(uri))};

        m_id_to_connection_map[new_id] = metadata_ptr;

        connection->set_open_handler([this, metadata_ptr](websocketpp::connection_hdl hdl) {
                metadata_ptr->on_open(&m_endpoint, hdl);
                this->m_logger->info("Client connection opened for ID: {}", metadata_ptr->get_id());
            });

        connection->set_fail_handler([this, metadata_ptr](websocketpp::connection_hdl hdl) {
            metadata_ptr->on_fail(&m_endpoint, hdl);
            this->m_logger->error("Connection failed for ID: {}. Check URI or Network.",
                                  metadata_ptr->get_id());
        });

        connection->set_close_handler([this, metadata_ptr](websocketpp::connection_hdl hdl) {
            metadata_ptr->on_close(&m_endpoint, hdl);
            this->m_logger->info("Connection closed for ID: {}", metadata_ptr->get_id());
            this->m_id_to_connection_map.erase(metadata_ptr->get_id());
        });

        connection->set_message_handler([this, metadata_ptr](websocketpp::connection_hdl hdl, Client::message_ptr msg) {
            metadata_ptr->on_message(hdl, msg);
            this->m_logger->info("Message received for ID: {}", metadata_ptr->get_id());
        });

        m_endpoint.connect(connection);

        return new_id;
    }

    private:
        std::atomic<bool> m_perpetual_active{false};
};

} // namespace transport
