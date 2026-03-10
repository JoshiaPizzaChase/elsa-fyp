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
        m_endpoint.stop_perpetual();
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

    /*
     * Stops websocket client gracefully.
     */
    std::expected<void, int> stop() override {
        m_logger->info("Manually stopping websocket manager client...");

        m_endpoint.stop_perpetual();

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
    std::expected<int, int> connect(std::string_view uri) {
        websocketpp::lib::error_code error_code;

        Client::connection_ptr connection =
            m_endpoint.get_connection(static_cast<std::string>(uri), error_code);

        if (error_code) {
            m_logger->error("Connect initialization error: {}", error_code.message());
            return std::unexpected{-1};
        }

        int new_id = m_next_id++;

        ConnectionMetadata::conn_meta_shared_ptr metadata_ptr{std::make_shared<ConnectionMetadata>(
            new_id, connection->get_handle(), static_cast<std::string>(uri))};

        m_id_to_connection_map[new_id] = metadata_ptr;

        connection->set_open_handler([metadata_ptr, capture0 = &m_endpoint](auto&& PH1) {
            metadata_ptr->on_open(capture0, std::forward<decltype(PH1)>(PH1));
        });
        connection->set_fail_handler([metadata_ptr, capture0 = &m_endpoint](auto&& PH1) {
            metadata_ptr->on_fail(capture0, std::forward<decltype(PH1)>(PH1));
        });
        connection->set_close_handler([metadata_ptr, capture0 = &m_endpoint](auto&& PH1) {
            metadata_ptr->on_close(capture0, std::forward<decltype(PH1)>(PH1));
        });
        connection->set_message_handler([metadata_ptr](auto&& PH1, auto&& PH2) {
            metadata_ptr->on_message(std::forward<decltype(PH1)>(PH1),
                                     std::forward<decltype(PH2)>(PH2));
        });

        m_endpoint.connect(connection);

        return new_id;
    }
};

} // namespace transport
