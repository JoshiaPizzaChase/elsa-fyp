#include "request_handler.h"
#include "queries.h"
#include <iostream>

namespace backend {

http::response<http::string_body> RequestHandler::handle(const http::request<http::string_body>& req) {
    std::string target = std::string(req.target());
    boost::urls::url_view url(target);
    std::string path(url.path());
    auto params = url.params();

    json::object res;
    http::status status = http::status::ok;

    if (req.method() == http::verb::get) {
        if (path == "/login") {
            res = handle_login(params);
        } else if (path == "/active_servers") {
            res = handle_active_servers();
        } else if (path == "/user_info") {
            res = handle_user_info(params);
        } else if (path == "/active_symbols") {
            res = handle_active_symbols(params);
        } else {
            status = http::status::not_found;
            res["error"] = "Unknown endpoint";
        }
    } else {
        status = http::status::method_not_allowed;
        res["error"] = "Only GET supported";
    }

    http::response<http::string_body> response{status, req.version()};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::access_control_allow_origin, "*");
    response.body() = json::serialize(res);
    response.prepare_payload();
    return response;
}

json::object RequestHandler::handle_login(const boost::urls::params_view& params) {
    json::object res;

    auto user_name_it = params.find("user_name");
    auto password_it = params.find("password");
    if (user_name_it == params.end() || password_it == params.end()) {
        res["success"] = false;
        res["err_msg"] = "Missing user_name or password";
        return res;
    }

    std::string user_name = (*user_name_it).value;
    std::string password = (*password_it).value;

    // TODO: execute queries::LOGIN with params {user_name, password}
    //       if result has 1 row -> success, else -> fail
    res["success"] = false;
    res["err_msg"] = "Not implemented";
    return res;
}

json::object RequestHandler::handle_active_servers() {
    json::object res;

    // TODO: execute queries::ACTIVE_SERVERS
    //       build json array from result rows
    res["servers"] = json::array();
    return res;
}

json::object RequestHandler::handle_user_info(const boost::urls::params_view& params) {
    json::object res;

    auto user_name_it = params.find("user_name");
    if (user_name_it == params.end()) {
        res["error"] = "Missing user_name";
        return res;
    }

    std::string user_name = (*user_name_it).value;

    // TODO: execute queries::USER_INFO with params {user_name}
    //       then execute queries::USER_BALANCES with the returned user_id
    res["user"] = nullptr;
    res["balances"] = json::array();
    return res;
}

json::object RequestHandler::handle_active_symbols(const boost::urls::params_view& params) {
    json::object res;

    auto server_name_it = params.find("server_name");
    if (server_name_it == params.end()) {
        res["error"] = "Missing server_name";
        return res;
    }

    std::string server_name = (*server_name_it).value;

    // TODO: execute queries::ACTIVE_SYMBOLS with params {server_name}
    res["symbols"] = json::array();
    return res;
}

} // namespace backend

