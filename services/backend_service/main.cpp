#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <print>
#include "rfl/toml/load.hpp"
#include "configuration/backend_config.h"
#include "request_handler.h"
#include "uat_seeder.h"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

static boost::asio::ip::tcp::acceptor* g_acceptor_ptr = nullptr;

void signal_handler(int) {
    if (g_acceptor_ptr) {
        boost::system::error_code ec;
        g_acceptor_ptr->close(ec); // unblocks the synchronous accept() call
    }
}

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
    auto backend_cfg = argc == 2 ? argv[1] : "backend.toml";
    try {
        backend::BackendConfig backend_config = rfl::toml::load<backend::BackendConfig>(backend_cfg).value();
        backend::RequestHandler handler;

        std::unique_ptr<backend::UatSeeder> uat_seeder;
        if (backend_config.uat) {
            uat_seeder = std::make_unique<backend::UatSeeder>(handler.get_db_client());
            uat_seeder->seed();
        }

        boost::asio::io_context ioc{1};

        std::signal(SIGINT,  signal_handler);
        std::signal(SIGTERM, signal_handler);

        tcp::acceptor acceptor{ioc, {tcp::v4(), static_cast<unsigned short>(backend_config.port)}};
        g_acceptor_ptr = &acceptor;
        std::println("Server started on port {}", backend_config.port);
        if (backend_config.uat)
            std::println("[UAT] Running in UAT mode — test data populated");

        try {
            for (;;) {
                tcp::socket socket{ioc};
                acceptor.accept(socket);
                std::thread([s = std::move(socket), &handler]() mutable {
                    handle_session(std::move(s), handler);
                }).detach();
            }
        } catch (const boost::system::system_error&) {
            // Expected: io_context was stopped via signal handler.
        }

        if (uat_seeder) {
            uat_seeder->cleanup();
        }

    } catch (std::exception& e) {
        std::println("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
