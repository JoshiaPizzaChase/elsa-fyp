#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

namespace backend {

class RequestHandler {
public:
    // Takes a parsed HTTP request, returns an HTTP response
    http::response<http::string_body> handle(const http::request<http::string_body>& req);

private:
    // Individual endpoint handlers
    json::object handle_login(const boost::urls::params_view& params);
    json::object handle_signup(const boost::urls::params_view& params);
    json::object handle_active_servers();
    json::object handle_user_info(const boost::urls::params_view& params);
    json::object handle_active_symbols(const boost::urls::params_view& params);
    json::object handle_user_servers(const boost::urls::params_view& params);
    json::object handle_historical_trades(const boost::urls::params_view& params);
};

} // namespace backend

