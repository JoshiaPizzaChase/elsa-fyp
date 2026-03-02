#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <print>
#include "rfl/toml/load.hpp"
#include "configuration/backend_config.h"
#include "request_handler.h"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

// Handles a single HTTP session
void handle_session(tcp::socket socket, backend::RequestHandler& handler) {
    try {
        boost::beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        auto response = handler.handle(req);
        http::write(socket, response);
    } catch (std::exception& e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println("Usage: backend_service <toml config path>");
        return -1;
    }
    try {
        backend::BackendConfig backend_config = rfl::toml::load<backend::BackendConfig>(argv[1]).value();
        backend::RequestHandler handler;

        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), backend_config.port}};
        std::println("Server started on port {}", backend_config.port);
        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread([s = std::move(socket), &handler]() mutable {
                handle_session(std::move(s), handler);
            }).detach();
        }
    } catch (std::exception& e) {
        std::println("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
