#define ENSURE_NO_BLOCKING

#include "websocket_client.h"
#include "websocket_server.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

static bool wait_for_open(transport::WebsocketManager<transport::Client>& mgr, int id,
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

static bool wait_for_open(transport::WebsocketManager<transport::Server>& mgr, int id,
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
    constexpr int port = 9003;
    const std::string uri = "localhost";

    transport::WebsocketManagerServer server(port, uri);
    if (!server.start().has_value()) {
        std::cerr << "Failed to start server" << std::endl;
        return -1;
    }
    std::cout << "Server started on port " << port << std::endl;
    std::this_thread::sleep_for(500ms); // Give server time to start

    // Start two clients
    transport::WebsocketManagerClient client_a{"client_a_logger"};
    transport::WebsocketManagerClient client_b{"client_b_logger"};
    if (!client_a.start().has_value() || !client_b.start().has_value()) {
        std::cerr << "Failed to start clients" << std::endl;
        return -1;
    }
    std::cout << "Clients started" << std::endl;
    std::this_thread::sleep_for(500ms); // Give clients time to start

    std::string ws_uri = std::format("ws://localhost:{}", port);
    auto a_res = client_a.connect(ws_uri);
    auto b_res = client_b.connect(ws_uri);
    if (!a_res.has_value() || !b_res.has_value()) {
        std::cerr << "Clients failed to connect" << std::endl;
        return -1;
    }
    std::cout << "Clients connecting to " << ws_uri << std::endl;

    int a_id = *a_res;
    int b_id = *b_res;
    std::cout << "Client A id for server: " << a_id << ", Client B id for server: " << b_id
              << std::endl;

    if (!wait_for_open(client_a, a_id) || !wait_for_open(client_b, b_id)) {
        std::cerr << "One or more clients did not open in time" << std::endl;
        return -1;
    }
    std::cout << "Both clients connected successfully" << std::endl;

    // Server ids for first two connections are 0 and 1
    if (!wait_for_open(server, 0) || !wait_for_open(server, 1)) {
        std::cerr << "Server did not register client connections in time" << std::endl;
        return -1;
    }
    std::cout << "Server registered both client connections with id 0 for A, id 1 for B"
              << std::endl;

    // Client A sends message
    const std::string msg_from_a = "hello_from_client_a";
    if (!client_a.send(a_id, msg_from_a).has_value()) {
        std::cerr << "Client A failed to send message" << std::endl;
        return -1;
    }
    std::cout << "Client A sent message to server" << std::endl;

    // Client B sends message
    const std::string msg_from_b = "hello_from_client_b";
    if (!client_b.send(b_id, msg_from_b).has_value()) {
        std::cerr << "Client B failed to send message" << std::endl;
        return -1;
    }
    std::cout << "Client B sent message to server" << std::endl;

// Server should receive it on one of the ids (0 or 1)
#ifdef ENSURE_NO_BLOCKING
    std::this_thread::sleep_for(500ms); // Give server time to receive messages
#endif
    auto opt0 = server.dequeue_message(0);
    auto opt1 = server.dequeue_message(1);

    std::cout << "Messages: " << (opt0.has_value() ? *opt0 : "none") << ", "
              << (opt1.has_value() ? *opt1 : "none") << std::endl;

    if (!opt0.has_value() && !opt1.has_value()) {
        std::cerr << "Server did not receive any messages from clients. Block waiting..."
                  << std::endl;

        // try a blocking wait briefly on each
        auto wait0 = server.wait_and_dequeue_message(0);
        auto wait1 = server.wait_and_dequeue_message(1);
        if (!wait0.has_value() && !wait1.has_value()) {
            std::cerr << "Server did not receive client message" << std::endl;
            return -1;
        }
    }
    std::cout << "Server received messages from clients" << std::endl;

    // Broadcast from server to all clients
    const std::string broadcast = "broadcast_from_server";
    auto broad_res = server.send_to_all(broadcast, transport::MessageFormat::text);
    if (!broad_res.has_value()) {
        std::cerr << "Broadcast failed to some ids" << std::endl;
        return -1;
    }
    std::cout << "Server broadcasted message to all clients" << std::endl;

    auto a_recv = client_a.wait_and_dequeue_message(a_id);
    auto b_recv = client_b.wait_and_dequeue_message(b_id);

    if (!a_recv.has_value() || !b_recv.has_value()) {
        std::cerr << "One of the clients did not receive broadcast" << std::endl;
        return -1;
    }

    if (*a_recv != broadcast || *b_recv != broadcast) {
        std::cerr << "Broadcast payload mismatch: a=" << *a_recv << " b=" << *b_recv << std::endl;
        return -1;
    }
    std::cout << "Both clients received broadcast successfully" << std::endl;

    // Clean up
    client_a.stop();
    client_b.stop();
    server.stop();

    std::cout << "test_server_2 passed" << std::endl;
    return 0;
}
