#include "request_handler.h"
#include <iostream>

namespace backend {

http::response<http::string_body> RequestHandler::handle(const http::request<http::string_body>& req) {
    // Handle CORS preflight
    if (req.method() == http::verb::options) {
        http::response<http::string_body> response{http::status::no_content, req.version()};
        response.set(http::field::access_control_allow_origin, "*");
        response.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
        response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
        response.prepare_payload();
        return response;
    }

    std::string target = std::string(req.target());
    boost::urls::url_view url(target);
    std::string path(url.path());
    auto params = url.params();

    bj::object res;
    http::status status = http::status::ok;

    if (req.method() == http::verb::get) {
        if (path == "/login") {
            res = handle_login(params);
        } else if (path == "/signup") {
            res = handle_signup(params);
        } else if (path == "/active_servers") {
            res = handle_active_servers();
        } else if (path == "/user_info") {
            res = handle_user_info(params);
        } else if (path == "/active_symbols") {
            res = handle_active_symbols(params);
        } else if (path == "/user_servers") {
            res = handle_user_servers(params);
        } else if (path == "/get_account_details") {
            res = handle_account_details(params);
        } else if (path == "/get_historical_trades") {
            res = handle_historical_trades(params);
        } else {
            status = http::status::not_found;
            res["error"] = "Unknown endpoint";
        }
    } else if (req.method() == http::verb::post) {
        if (path == "/create_server") {
            res = handle_create_server(req);
            if (res.contains("error")) status = http::status::bad_request;
            if (res.contains("auth_error")) status = http::status::unauthorized;
        } else if (path == "/configure_server") {
            res = handle_configure_server(req);
            if (res.contains("error")) status = http::status::bad_request;
            if (res.contains("auth_error")) status = http::status::unauthorized;
        } else {
            status = http::status::not_found;
            res["error"] = "Unknown endpoint";
        }
    } else {
        status = http::status::method_not_allowed;
        res["error"] = "Method not allowed";
    }

    http::response<http::string_body> response{status, req.version()};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::access_control_allow_origin, "*");
    response.body() = bj::serialize(res);
    response.prepare_payload();
    return response;
}

bj::object RequestHandler::handle_login(const boost::urls::params_view& params) {
    bj::object res;

    auto user_name_it = params.find("user_name");
    auto password_it  = params.find("password");
    if (user_name_it == params.end() || password_it == params.end()) {
        res["success"] = false;
        res["err_msg"] = "Missing user_name or password";
        return res;
    }

    const std::string user_name = (*user_name_it).value;
    const std::string password  = (*password_it).value;

    auto result = m_db_client.authenticate_user(user_name, password);
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["success"] = false;
        res["err_msg"] = "Internal server error";
        return res;
    }

    if (result.value().has_value()) {
        res["success"] = true;
        res["err_msg"] = "";
    } else {
        res["success"] = false;
        res["err_msg"] = "Invalid username or password";
    }
    return res;
}

bj::object RequestHandler::handle_signup(const boost::urls::params_view& params) {
    bj::object res;

    auto user_name_it = params.find("user_name");
    auto password_it  = params.find("password");
    if (user_name_it == params.end() || password_it == params.end()) {
        res["success"] = false;
        res["err_msg"] = "Missing user_name or password";
        return res;
    }

    const std::string user_name = (*user_name_it).value;
    const std::string password  = (*password_it).value;

    auto result = m_db_client.create_user(user_name, password);
    if (!result.has_value()) {
        // Likely a duplicate username constraint violation
        res["success"] = false;
        res["err_msg"] = "Username already exists";
        return res;
    }

    res["success"] = true;
    res["err_msg"] = "";
    return res;
}

bj::object RequestHandler::handle_active_servers() {
    bj::object res;

    auto result = m_db_client.get_active_servers();
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }

    bj::array arr;
    for (const auto& srv : result.value()) {
        bj::object obj;
        obj["server_id"]   = srv.server_id;
        obj["server_name"] = srv.server_name;
        obj["admin_name"]  = srv.admin_name;
        obj["description"] = srv.description;

        bj::array symbols;
        for (const auto& s : srv.active_tickers) symbols.emplace_back(s);
        obj["active_symbols"] = std::move(symbols);

        arr.emplace_back(std::move(obj));
    }

    res["servers"] = std::move(arr);
    return res;
}

bj::object RequestHandler::handle_user_info(const boost::urls::params_view& params) {
    bj::object res;

    auto user_name_it = params.find("user_name");
    if (user_name_it == params.end()) {
        res["error"] = "Missing user_name";
        return res;
    }

    const std::string user_name = (*user_name_it).value;

    auto user_result = m_db_client.get_user(user_name);
    if (!user_result.has_value()) {
        std::cerr << user_result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }
    if (!user_result.value().has_value()) {
        res["error"] = "User not found";
        return res;
    }

    const auto& user = user_result.value().value();

    bj::object user_obj;
    user_obj["user_id"]  = user.user_id;
    user_obj["username"] = user.username;
    res["user"] = std::move(user_obj);

    auto bal_result = m_db_client.read_balances(user.user_id);
    if (!bal_result.has_value()) {
        std::cerr << bal_result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }

    bj::array bal_arr;
    for (const auto& b : bal_result.value()) {
        bj::object bo;
        bo["symbol"]  = b.symbol;
        bo["balance"] = b.balance;
        bal_arr.emplace_back(std::move(bo));
    }
    res["balances"] = std::move(bal_arr);
    return res;
}

bj::object RequestHandler::handle_active_symbols(const boost::urls::params_view& params) {
    bj::object res;

    auto server_name_it = params.find("server_name");
    if (server_name_it == params.end()) {
        res["error"] = "Missing server_name";
        return res;
    }

    const std::string server_name = (*server_name_it).value;

    auto result = m_db_client.get_server_active_symbols(server_name);
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }

    bj::array symbols;
    for (const auto& s : result.value()) symbols.emplace_back(s);
    res["symbols"] = std::move(symbols);
    return res;
}

bj::object RequestHandler::handle_user_servers(const boost::urls::params_view& params) {
    bj::object res;

    auto user_name_it = params.find("user_name");
    if (user_name_it == params.end()) {
        res["error"] = "Missing user_name";
        return res;
    }

    const std::string user_name = (*user_name_it).value;

    auto result = m_db_client.get_user_servers(user_name);
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }

    if (result.value().empty()) {
        // Could be user not found or simply no servers — check user existence
        auto user_result = m_db_client.get_user(user_name);
        if (!user_result.has_value() || !user_result.value().has_value()) {
            res["error"] = "User not found";
            res["servers"] = bj::array();
            return res;
        }
    }

    bj::array arr;
    for (const auto& srv : result.value()) {
        bj::object obj;
        obj["server_id"]   = srv.server_id;
        obj["server_name"] = srv.server_name;
        obj["role"]        = srv.role;
        obj["description"] = srv.description;

        bj::array symbols;
        for (const auto& s : srv.active_tickers) symbols.emplace_back(s);
        obj["active_symbols"] = std::move(symbols);

        bj::array bal_arr;
        for (const auto& b : srv.balances) {
            bj::object bo;
            bo["symbol"]  = b.symbol;
            bo["balance"] = b.balance;
            bal_arr.emplace_back(std::move(bo));
        }
        obj["balances"] = std::move(bal_arr);
        arr.emplace_back(std::move(obj));
    }

    res["servers"] = std::move(arr);
    return res;
}

bj::object RequestHandler::handle_historical_trades(const boost::urls::params_view& params) {
    bj::object res;

    auto server_it = params.find("server");
    auto symbol_it = params.find("symbol");
    if (server_it == params.end() || symbol_it == params.end()) {
        res["error"] = "Missing server or symbol";
        return res;
    }

    const std::string symbol = (*symbol_it).value;

    auto result = m_db_client.get_historical_trades(symbol);
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }

    bj::array trades;
    for (const auto& t : result.value()) {
        bj::object trade;
        trade["trade_id"]         = t.trade_id;
        trade["ticker"]           = t.symbol;
        trade["price"]            = t.price;
        trade["quantity"]         = t.quantity;
        trade["create_timestamp"] = t.ts_ms;
        trades.emplace_back(std::move(trade));
    }

    res["trades"] = std::move(trades);
    return res;
}

bj::object RequestHandler::handle_account_details(const boost::urls::params_view& params) {
    bj::object res;

    auto user_name_it   = params.find("user_name");
    auto server_name_it = params.find("server_name");
    if (user_name_it == params.end() || server_name_it == params.end()) {
        res["error"] = "Missing user_name or server_name";
        return res;
    }

    const std::string user_name   = (*user_name_it).value;
    const std::string server_name = (*server_name_it).value;

    auto result = m_db_client.get_account_details(user_name, server_name);
    if (!result.has_value()) {
        std::cerr << result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }
    if (!result.value().has_value()) {
        res["error"] = "User is not a member of this server";
        return res;
    }

    const auto& details = result.value().value();

    bj::object server_obj;
    server_obj["server_id"]   = details.server_id;
    server_obj["server_name"] = details.server_name;
    server_obj["description"] = details.description;
    server_obj["admin_name"]  = details.admin_name;

    bj::array ticker_arr;
    for (const auto& t : details.active_tickers) ticker_arr.emplace_back(t);
    server_obj["active_symbols"] = std::move(ticker_arr);
    res["server"] = std::move(server_obj);

    res["role"] = details.role;

    bj::array bal_arr;
    int total_value = 0;
    for (const auto& b : details.balances) {
        bj::object bo;
        bo["symbol"]  = b.symbol;
        bo["balance"] = b.balance;
        bal_arr.emplace_back(std::move(bo));
        total_value += b.balance;
    }
    res["balances"]    = std::move(bal_arr);
    res["total_value"] = total_value;

    constexpr int INITIAL_BALANCE = 100000;
    res["pnl"]     = total_value - INITIAL_BALANCE;
    res["pnl_pct"] = static_cast<double>(total_value - INITIAL_BALANCE) / INITIAL_BALANCE * 100.0;

    return res;
}

// ---------------------------------------------------------------------------
// Auth helper
// ---------------------------------------------------------------------------
// Simple scheme: the frontend sends  "Authorization: Bearer <username>"
// This lets us identify who is making the request without a real JWT stack.
int RequestHandler::authenticate_admin(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return -1;

    std::string value = std::string(it->value());
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) != 0) return -1;

    const std::string username = value.substr(prefix.size());
    auto result = m_db_client.get_user(username);
    if (!result.has_value() || !result.value().has_value()) return -1;
    return result.value()->user_id;
}

// ---------------------------------------------------------------------------
// POST /create_server
// ---------------------------------------------------------------------------
bj::object RequestHandler::handle_create_server(const http::request<http::string_body>& req) {
    bj::object res;

    const int caller_id = authenticate_admin(req);
    if (caller_id < 0) {
        res["auth_error"] = "Unauthorized: valid Authorization header required";
        return res;
    }

    bj::value body_val;
    try {
        body_val = bj::parse(req.body());
    } catch (...) {
        res["error"] = "Invalid JSON body";
        return res;
    }
    if (!body_val.is_object()) {
        res["error"] = "Body must be a JSON object";
        return res;
    }
    const auto& body = body_val.as_object();

    auto name_it = body.find("server_name");
    if (name_it == body.end() || !name_it->value().is_string()) {
        res["error"] = "Missing or invalid server_name";
        return res;
    }
    const std::string server_name = std::string(name_it->value().as_string());

    std::string description;
    if (auto it = body.find("description"); it != body.end() && it->value().is_string())
        description = std::string(it->value().as_string());

    std::vector<std::string> symbols;
    if (auto it = body.find("active_symbols"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array())
            if (s.is_string()) symbols.emplace_back(std::string(s.as_string()));
    }

    std::vector<std::string> allowlist_names;
    if (auto it = body.find("allowlist"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array())
            if (s.is_string()) allowlist_names.emplace_back(std::string(s.as_string()));
    }

    // Resolve allowlist usernames to user IDs
    auto ids_result = m_db_client.resolve_user_ids(allowlist_names);
    if (!ids_result.has_value()) {
        res["error"] = ids_result.error();
        return res;
    }

    auto create_result = m_db_client.create_server(
        server_name, caller_id, description, symbols, ids_result.value());
    if (!create_result.has_value()) {
        res["error"] = create_result.error();
        return res;
    }

    res["success"]    = true;
    res["server_id"]  = create_result.value();
    res["server_name"] = server_name;
    res["admin_id"]   = caller_id;
    res["description"] = description;

    bj::array sym_arr;
    for (const auto& s : symbols) sym_arr.emplace_back(s);
    res["active_symbols"] = std::move(sym_arr);

    bj::array al_arr;
    for (const auto& u : allowlist_names) al_arr.emplace_back(u);
    res["allowlist"] = std::move(al_arr);

    return res;
}

// ---------------------------------------------------------------------------
// POST /configure_server
// ---------------------------------------------------------------------------
bj::object RequestHandler::handle_configure_server(const http::request<http::string_body>& req) {
    bj::object res;

    const int caller_id = authenticate_admin(req);
    if (caller_id < 0) {
        res["auth_error"] = "Unauthorized: valid Authorization header required";
        return res;
    }

    bj::value body_val;
    try {
        body_val = bj::parse(req.body());
    } catch (...) {
        res["error"] = "Invalid JSON body";
        return res;
    }
    if (!body_val.is_object()) {
        res["error"] = "Body must be a JSON object";
        return res;
    }
    const auto& body = body_val.as_object();

    auto name_it = body.find("server_name");
    if (name_it == body.end() || !name_it->value().is_string()) {
        res["error"] = "Missing or invalid server_name";
        return res;
    }
    const std::string server_name = std::string(name_it->value().as_string());

    // Fetch current server state so we can fall back to existing values
    auto srv_result = m_db_client.get_server(server_name);
    if (!srv_result.has_value()) {
        std::cerr << srv_result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }
    if (!srv_result.value().has_value()) {
        res["error"] = "Server not found: " + server_name;
        return res;
    }
    const auto& srv = srv_result.value().value();

    if (srv.admin_id != caller_id) {
        res["auth_error"] = "Forbidden: you are not the admin of this server";
        return res;
    }

    std::string description = srv.description;
    if (auto it = body.find("description"); it != body.end() && it->value().is_string())
        description = std::string(it->value().as_string());

    std::vector<std::string> symbols = srv.active_tickers;
    if (auto it = body.find("active_symbols"); it != body.end() && it->value().is_array()) {
        symbols.clear();
        for (const auto& s : it->value().as_array())
            if (s.is_string()) symbols.emplace_back(std::string(s.as_string()));
    }

    // Allowlist: use provided list, or keep existing by querying current members
    std::vector<std::string> allowlist_names;
    if (auto it = body.find("allowlist"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array())
            if (s.is_string()) allowlist_names.emplace_back(std::string(s.as_string()));
    }

    auto ids_result = m_db_client.resolve_user_ids(allowlist_names);
    if (!ids_result.has_value()) {
        res["error"] = ids_result.error();
        return res;
    }

    auto cfg_result = m_db_client.configure_server(
        server_name, caller_id, description, symbols, ids_result.value());
    if (!cfg_result.has_value()) {
        res["error"] = cfg_result.error();
        return res;
    }

    res["success"]     = true;
    res["server_name"] = server_name;
    res["description"] = description;

    bj::array sym_arr;
    for (const auto& s : symbols) sym_arr.emplace_back(s);
    res["active_symbols"] = std::move(sym_arr);

    bj::array al_arr;
    for (const auto& u : allowlist_names) al_arr.emplace_back(u);
    res["allowlist"] = std::move(al_arr);

    return res;
}

} // namespace backend
