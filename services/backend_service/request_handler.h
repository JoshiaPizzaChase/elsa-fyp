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
    json::object handle_account_details(const boost::urls::params_view& params);
    json::object handle_historical_trades(const boost::urls::params_view& params);
    json::object handle_create_server(const http::request<http::string_body>& req);
    json::object handle_configure_server(const http::request<http::string_body>& req);

    // Auth helper: returns the user_id of the requester if the
    // Authorization header carries a valid "Bearer <username>" token,
    // or -1 if the token is missing / invalid.
    int authenticate_admin(const http::request<http::string_body>& req);
};

} // namespace backend

