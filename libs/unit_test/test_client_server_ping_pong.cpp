#include "transport/websocket_client.h"
#include "transport/websocket_server.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

template <typename T>
static bool wait_for_open(transport::WebsocketManager<T>& mgr, int id,
                          std::chrono::milliseconds timeout = 5000ms) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        auto meta = mgr.get_metadata(id);
        if (meta && meta->get_status() == transport::ConnectionStatus::open) {
            return true;
        }
        std::this_thread::sleep_for(50ms);
    }
    return false;
}

int main() {
    constexpr int port = 9002;
    const std::string uri = "localhost";

    // Start server
    transport::WebsocketManagerServer server(port, uri);
    if (!server.start().has_value()) {
        std::cerr << "Failed to start server" << std::endl;
        return -1;
    }
    std::cout << "Server started on port " << port << std::endl;
    std::this_thread::sleep_for(500ms); // Give server time to start

    // Start client
    transport::WebsocketManagerClient client;
    if (!client.start().has_value()) {
        std::cerr << "Failed to start client" << std::endl;
        return -1;
    }
    std::cout << "Client started" << std::endl;
    std::this_thread::sleep_for(500ms); // Give client time to start

    std::string ws_uri = std::format("ws://localhost:{}", port);
    auto conn_res = client.connect(ws_uri);
    if (!conn_res.has_value()) {
        std::cerr << "Client failed to connect" << std::endl;
        return -1;
    }
    std::cout << "Client connecting to " << ws_uri << std::endl;

    int client_id = *conn_res;
    std::cout << "Client connection id: " << client_id << std::endl;

    // Wait for client and server connection to be open
    if (!wait_for_open(client, client_id)) {
        std::cerr << "Client connection did not open in time" << std::endl;
        return -1;
    }

    // Server side id should be 0 for first connection
    int server_id = 0;
    if (!wait_for_open(server, server_id)) {
        std::cerr << "Server did not register open connection in time" << std::endl;
        return -1;
    }
    std::cout << "Client and Server connections are open" << std::endl;

    // Client -> Server
    const std::string msg_to_server = "ping_from_client";
    if (!client.send(client_id, msg_to_server).has_value()) {
        std::cerr << "Client failed to send message" << std::endl;
        return -1;
    }

    auto opt_msg = server.wait_and_dequeue_message(server_id);
    if (!opt_msg.has_value()) {
        std::cerr << "Server did not receive message" << std::endl;
        return -1;
    }
    std::cout << "Server received message: " << *opt_msg << std::endl;

    if (*opt_msg != msg_to_server) {
        std::cerr << "Server received unexpected message: " << *opt_msg << std::endl;
        return -1;
    }

    // Server -> Client
    const std::string msg_to_client = "pong_from_server";
    if (!server.send(server_id, msg_to_client).has_value()) {
        std::cerr << "Server failed to send message to client" << std::endl;
        return -1;
    }

    auto opt_client_msg = client.wait_and_dequeue_message(client_id);
    if (!opt_client_msg.has_value()) {
        std::cerr << "Client did not receive message" << std::endl;
        return -1;
    }

    if (*opt_client_msg != msg_to_client) {
        std::cerr << "Client received unexpected message: " << *opt_client_msg << std::endl;
        return -1;
    }
    std::cout << "Client received message: " << *opt_client_msg << std::endl;

    // Clean up
    if (!client.stop().has_value()) {
        std::cerr << "Client stop failed" << std::endl;
    }
    if (!server.stop().has_value()) {
        std::cerr << "Server stop failed" << std::endl;
    }

    std::cout << "test_client_2 passed" << std::endl;
    return 0;
}
