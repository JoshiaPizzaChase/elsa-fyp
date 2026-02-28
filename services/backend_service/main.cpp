#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <sstream>
#include <print>
#include "rfl/toml/load.hpp"
#include "configuration/backend_config.h"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;
namespace json = boost::json;

// Helper to parse query params from target string
std::map<std::string, std::string> parse_query(const std::string& target) {
    std::map<std::string, std::string> params;
    auto pos = target.find('?');
    if (pos == std::string::npos) return params;
    std::string query = target.substr(pos + 1);
    std::istringstream iss(query);
    std::string kv;
    while (std::getline(iss, kv, '&')) {
        auto eq = kv.find('=');
        if (eq != std::string::npos) {
            params[kv.substr(0, eq)] = kv.substr(eq + 1);
        }
    }
    return params;
}

// Handles a single HTTP session
void handle_session(tcp::socket socket) {
    try {
        boost::beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        std::string target = std::string(req.target());
        std::string path = target.substr(0, target.find('?'));
        auto params = parse_query(target);
        json::object res;

        if (req.method() == http::verb::get) {
            if (path == "/login") {
                res["success"] = false;
                res["err_msg"] = "Not implemented";
            } else if (path == "/active_servers") {
                res["servers"] = json::array();
            } else if (path == "/user_info") {
                res["user"] = nullptr;
            } else if (path == "/active_symbols") {
                res["symbols"] = json::array();
            } else {
                res["error"] = "Unknown endpoint";
            }
        } else {
            res["error"] = "Only GET supported";
        }

        http::response<http::string_body> response{http::status::ok, req.version()};
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(res);
        response.prepare_payload();
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
        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), backend_config.port}};
        std::print("Server started on port {}",backend_config.port);
        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            // Use lambda to move socket into thread
            std::thread([s = std::move(socket)]() mutable { handle_session(std::move(s)); }).detach();
        }
    } catch (std::exception& e) {
        std::print("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
