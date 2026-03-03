#include "request_handler.h"
#include "queries.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace backend {

// hardcoded demo data
struct UserRecord {
    int user_id;
    std::string username;
    std::string password;
};

struct ServerRecord {
    int server_id;
    std::string server_name;
    int admin_id;
    std::vector<std::string> active_symbols;
    std::string mdp_endpoint;
    std::string description;
};

struct BalanceRecord {
    int user_id;
    std::string symbol;
    int balance;
};

struct AllowlistEntry {
    int server_id;
    int user_id;
};

static const std::vector<UserRecord> USERS = {
    {1, "alice",   "password"},
    {2, "bob",     "password"},
    {3, "charlie", "password"},
};

static const std::vector<ServerRecord> SERVERS = {
    {1, "hk01",  1, {"AAPL", "GOOGL", "MSFT", "AMZN"}, "ws://localhost:9001", "Hong Kong equities sandbox — US tech names."},
    {2, "us01",  2, {"TSLA", "META", "NVDA", "JPM"},    "ws://localhost:9002", "US equities sandbox — EV, social media & finance."},
    {3, "eu01",  1, {"AAPL", "TSLA"},                    "ws://localhost:9003", "European session sandbox — limited symbol set."},
};

static const std::vector<BalanceRecord> BALANCES = {
    {1, "USD",   50000},
    {1, "AAPL",  15000},
    {1, "GOOGL", 10000},
    {1, "MSFT",  12000},
    {1, "TSLA",   8000},
    {2, "USD",   60000},
    {2, "TSLA",  20000},
    {2, "META",  10000},
    {3, "USD",   40000},
    {3, "AAPL",   5000},
};

static const std::vector<AllowlistEntry> ALLOWLIST = {
    {1, 2},  // bob is member of hk01
    {1, 3},  // charlie is member of hk01
    {2, 1},  // alice is member of us01
    {3, 3},  // charlie is member of eu01
};

// helpers
static const UserRecord* find_user(const std::string& username) {
    for (auto& u : USERS) {
        if (u.username == username) return &u;
    }
    return nullptr;
}

static const UserRecord* find_user_by_id(int user_id) {
    for (auto& u : USERS) {
        if (u.user_id == user_id) return &u;
    }
    return nullptr;
}

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

    json::object res;
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

    const auto* user = find_user(user_name);
    if (user && user->password == password) {
        res["success"] = true;
        res["err_msg"] = "";
    } else {
        res["success"] = false;
        res["err_msg"] = "Invalid username or password";
    }
    return res;
}

json::object RequestHandler::handle_signup(const boost::urls::params_view& params) {
    json::object res;

    auto user_name_it = params.find("user_name");
    auto password_it = params.find("password");
    if (user_name_it == params.end() || password_it == params.end()) {
        res["success"] = false;
        res["err_msg"] = "Missing user_name or password";
        return res;
    }

    // TODO: INSERT INTO users (username, password) VALUES ($1, $2)
    // return error if username already exists
    res["success"] = true;
    res["err_msg"] = "";
    return res;
}

json::object RequestHandler::handle_active_servers() {
    json::object res;
    json::array arr;

    for (auto& srv : SERVERS) {
        json::object obj;
        obj["server_id"] = srv.server_id;
        obj["server_name"] = srv.server_name;
        const auto* admin = find_user_by_id(srv.admin_id);
        obj["admin_name"] = admin ? admin->username : "unknown";

        json::array symbols;
        for (auto& s : srv.active_symbols) symbols.emplace_back(s);
        obj["active_symbols"] = std::move(symbols);

        obj["mdp_endpoint"] = srv.mdp_endpoint;
        obj["description"] = srv.description;
        arr.emplace_back(std::move(obj));
    }

    res["servers"] = std::move(arr);
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
    const auto* user = find_user(user_name);

    if (!user) {
        res["error"] = "User not found";
        return res;
    }

    json::object user_obj;
    user_obj["user_id"] = user->user_id;
    user_obj["username"] = user->username;
    res["user"] = std::move(user_obj);

    json::array bal_arr;
    for (auto& b : BALANCES) {
        if (b.user_id == user->user_id) {
            json::object bo;
            bo["symbol"] = b.symbol;
            bo["balance"] = b.balance;
            bal_arr.emplace_back(std::move(bo));
        }
    }
    res["balances"] = std::move(bal_arr);
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

    for (auto& srv : SERVERS) {
        if (srv.server_name == server_name) {
            json::array symbols;
            for (auto& s : srv.active_symbols) symbols.emplace_back(s);
            res["symbols"] = std::move(symbols);
            return res;
        }
    }

    res["symbols"] = json::array();
    return res;
}

json::object RequestHandler::handle_user_servers(const boost::urls::params_view& params) {
    json::object res;

    auto user_name_it = params.find("user_name");
    if (user_name_it == params.end()) {
        res["error"] = "Missing user_name";
        return res;
    }

    std::string user_name = (*user_name_it).value;
    const auto* user = find_user(user_name);

    if (!user) {
        res["error"] = "User not found";
        res["servers"] = json::array();
        return res;
    }

    json::array arr;

    // Servers where user is admin
    for (auto& srv : SERVERS) {
        if (srv.admin_id == user->user_id) {
            json::object obj;
            obj["server_id"] = srv.server_id;
            obj["server_name"] = srv.server_name;
            obj["role"] = "admin";

            json::array symbols;
            for (auto& s : srv.active_symbols) symbols.emplace_back(s);
            obj["active_symbols"] = std::move(symbols);
            obj["description"] = srv.description;

            // Balances for this user
            json::array bal_arr;
            for (auto& b : BALANCES) {
                if (b.user_id == user->user_id) {
                    json::object bo;
                    bo["symbol"] = b.symbol;
                    bo["balance"] = b.balance;
                    bal_arr.emplace_back(std::move(bo));
                }
            }
            obj["balances"] = std::move(bal_arr);
            arr.emplace_back(std::move(obj));
        }
    }

    // Servers where user is member (via allowlist)
    for (auto& entry : ALLOWLIST) {
        if (entry.user_id == user->user_id) {
            for (auto& srv : SERVERS) {
                if (srv.server_id == entry.server_id) {
                    json::object obj;
                    obj["server_id"] = srv.server_id;
                    obj["server_name"] = srv.server_name;
                    obj["role"] = "member";

                    json::array symbols;
                    for (auto& s : srv.active_symbols) symbols.emplace_back(s);
                    obj["active_symbols"] = std::move(symbols);
                    obj["description"] = srv.description;

                    json::array bal_arr;
                    for (auto& b : BALANCES) {
                        if (b.user_id == user->user_id) {
                            json::object bo;
                            bo["symbol"] = b.symbol;
                            bo["balance"] = b.balance;
                            bal_arr.emplace_back(std::move(bo));
                        }
                    }
                    obj["balances"] = std::move(bal_arr);
                    arr.emplace_back(std::move(obj));
                    break;
                }
            }
        }
    }

    res["servers"] = std::move(arr);
    return res;
}

json::object RequestHandler::handle_historical_trades(const boost::urls::params_view& params) {
    json::object res;

    auto server_it = params.find("server");
    auto symbol_it = params.find("symbol");
    if (server_it == params.end() || symbol_it == params.end()) {
        res["error"] = "Missing server or symbol";
        return res;
    }

    std::string symbol = (*symbol_it).value;

    // TODO: query QuestDB with queries::GET_HISTORICAL_TRADES
    //       SELECT trade_id, symbol, price, quantity, ts FROM trades
    //       WHERE symbol = $1 AND ts >= dateadd('h', -2, now()) ORDER BY ts ASC
    //       Map each row to { trade_id, ticker, price, quantity, create_timestamp (epoch ms) }

    // Hardcoded: generate a random walk for the past 2 hours (~720 trades, one per 10 s)
    const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const long long two_hours_ms = 2LL * 60 * 60 * 1000;
    const long long start_ms = now_ms - two_hours_ms;
    const long long step_ms = 10'000;

    json::array trades;
    double price = 0.75;
    int trade_id = 1;

    // Use a simple deterministic seed based on symbol so different symbols
    // produce different-looking charts
    unsigned int seed = 0;
    for (char c : symbol) seed = seed * 31 + static_cast<unsigned char>(c);
    std::srand(seed);

    for (long long t = start_ms; t <= now_ms; t += step_ms) {
        double delta = ((std::rand() % 200) - 100) / 5000.0; // ±0.02
        price += delta;
        if (price < 0.5) price = 0.5;
        if (price > 1.0) price = 1.0;

        int qty = (std::rand() % 91) + 10; // 10–100

        json::object trade;
        trade["trade_id"]        = trade_id++;
        trade["ticker"]          = symbol;
        trade["price"]           = static_cast<double>(static_cast<int>(price * 10000)) / 10000.0;
        trade["quantity"]        = qty;
        trade["create_timestamp"] = t;
        trades.emplace_back(std::move(trade));
    }

    res["trades"] = std::move(trades);
    return res;
}

json::object RequestHandler::handle_account_details(const boost::urls::params_view& params) {
    json::object res;

    auto user_name_it  = params.find("user_name");
    auto server_name_it = params.find("server_name");
    if (user_name_it == params.end() || server_name_it == params.end()) {
        res["error"] = "Missing user_name or server_name";
        return res;
    }

    std::string user_name   = (*user_name_it).value;
    std::string server_name = (*server_name_it).value;

    const auto* user = find_user(user_name);
    if (!user) {
        res["error"] = "User not found";
        return res;
    }

    // Find the server
    const ServerRecord* srv = nullptr;
    for (auto& s : SERVERS) {
        if (s.server_name == server_name) { srv = &s; break; }
    }
    if (!srv) {
        res["error"] = "Server not found";
        return res;
    }

    // Determine role
    std::string role;
    if (srv->admin_id == user->user_id) {
        role = "admin";
    } else {
        for (auto& entry : ALLOWLIST) {
            if (entry.server_id == srv->server_id && entry.user_id == user->user_id) {
                role = "member";
                break;
            }
        }
    }
    if (role.empty()) {
        res["error"] = "User is not a member of this server";
        return res;
    }

    // Server info
    json::object server_obj;
    server_obj["server_id"]   = srv->server_id;
    server_obj["server_name"] = srv->server_name;
    server_obj["description"] = srv->description;
    const auto* admin = find_user_by_id(srv->admin_id);
    server_obj["admin_name"]  = admin ? admin->username : "unknown";
    json::array ticker_arr;
    for (auto& t : srv->active_symbols) ticker_arr.emplace_back(t);
    server_obj["active_symbols"] = std::move(ticker_arr);
    res["server"] = std::move(server_obj);

    res["role"] = role;

    // Balances filtered to the server's active symbols
    const auto& active = srv->active_symbols;
    json::array bal_arr;
    int total_value = 0;
    for (auto& b : BALANCES) {
        if (b.user_id != user->user_id) continue;
        bool in_server = (b.symbol == "USD");  // always include cash
        if (!in_server) {
            for (auto& sym : active) {
                if (sym == b.symbol) { in_server = true; break; }
            }
        }
        if (in_server) {
            json::object bo;
            bo["symbol"]  = b.symbol;
            bo["balance"] = b.balance;
            bal_arr.emplace_back(std::move(bo));
            total_value += b.balance;
        }
    }
    res["balances"]    = std::move(bal_arr);
    res["total_value"] = total_value;

    // Simple P&L vs a fixed starting balance of 100 000
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
// In a production system this should be replaced with signed tokens.
int RequestHandler::authenticate_admin(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return -1;

    std::string value = std::string(it->value());
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) != 0) return -1;

    std::string username = value.substr(prefix.size());
    const auto* u = find_user(username);
    return u ? u->user_id : -1;
}

// ---------------------------------------------------------------------------
// POST /create_server
// Body (JSON): { "user_name": "...", "server_name": "...", "description": "...",
//                "mdp_endpoint": "...", "active_symbols": ["A","B"],
//                "allowlist": ["user1","user2"] }
// Header:      Authorization: Bearer <username>
// ---------------------------------------------------------------------------
json::object RequestHandler::handle_create_server(const http::request<http::string_body>& req) {
    json::object res;

    int caller_id = authenticate_admin(req);
    if (caller_id < 0) {
        res["auth_error"] = "Unauthorized: valid Authorization header required";
        return res;
    }

    // Parse JSON body
    json::value body_val;
    try {
        body_val = json::parse(req.body());
    } catch (...) {
        res["error"] = "Invalid JSON body";
        return res;
    }
    if (!body_val.is_object()) {
        res["error"] = "Body must be a JSON object";
        return res;
    }
    const auto& body = body_val.as_object();

    // Required fields
    auto name_it = body.find("server_name");
    if (name_it == body.end() || !name_it->value().is_string()) {
        res["error"] = "Missing or invalid server_name";
        return res;
    }
    std::string server_name = std::string(name_it->value().as_string());

    // Optional fields with defaults
    std::string description;
    if (auto it = body.find("description"); it != body.end() && it->value().is_string())
        description = std::string(it->value().as_string());

    std::string mdp_endpoint = "ws://localhost:9999";
    if (auto it = body.find("mdp_endpoint"); it != body.end() && it->value().is_string())
        mdp_endpoint = std::string(it->value().as_string());

    // Active symbols
    std::vector<std::string> symbols;
    if (auto it = body.find("active_symbols"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array()) {
            if (s.is_string()) symbols.emplace_back(std::string(s.as_string()));
        }
    }

    // Allowlist usernames
    std::vector<std::string> allowlist;
    if (auto it = body.find("allowlist"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array()) {
            if (s.is_string()) allowlist.emplace_back(std::string(s.as_string()));
        }
    }

    // Validate all allowlist users exist
    for (const auto& uname : allowlist) {
        if (!find_user(uname)) {
            res["error"] = "User not found: " + uname;
            return res;
        }
    }

    // TODO: INSERT INTO servers (server_name, admin_id, description, mdp_endpoint) VALUES (...)
    //       INSERT INTO server_symbols (server_id, symbol) VALUES (...)  for each symbol
    //       INSERT INTO allowlist (server_id, user_id) VALUES (...)  for each allowlist user
    //
    // For now, echo back a success response with the submitted data so the
    // frontend can confirm the call worked.
    res["success"] = true;
    res["server_name"] = server_name;
    res["admin_id"]    = caller_id;
    res["description"] = description;
    res["mdp_endpoint"] = mdp_endpoint;

    json::array sym_arr;
    for (auto& s : symbols) sym_arr.emplace_back(s);
    res["active_symbols"] = std::move(sym_arr);

    json::array al_arr;
    for (auto& u : allowlist) al_arr.emplace_back(u);
    res["allowlist"] = std::move(al_arr);

    return res;
}

// ---------------------------------------------------------------------------
// POST /configure_server
// Body (JSON): { "server_name": "...", "description": "...",
//                "mdp_endpoint": "...", "active_symbols": ["A","B"],
//                "allowlist": ["user1","user2"] }
// Header:      Authorization: Bearer <username>
// Only the admin of the server (caller_id == server.admin_id) may configure it.
// ---------------------------------------------------------------------------
json::object RequestHandler::handle_configure_server(const http::request<http::string_body>& req) {
    json::object res;

    int caller_id = authenticate_admin(req);
    if (caller_id < 0) {
        res["auth_error"] = "Unauthorized: valid Authorization header required";
        return res;
    }

    json::value body_val;
    try {
        body_val = json::parse(req.body());
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
    std::string server_name = std::string(name_it->value().as_string());

    // Find the server in the in-memory data
    const ServerRecord* srv = nullptr;
    for (const auto& s : SERVERS) {
        if (s.server_name == server_name) { srv = &s; break; }
    }
    if (!srv) {
        res["error"] = "Server not found: " + server_name;
        return res;
    }

    // Permission check: only the admin of this server may configure it
    if (srv->admin_id != caller_id) {
        res["auth_error"] = "Forbidden: you are not the admin of this server";
        return res;
    }

    // Collect updated fields (fall back to current values when not supplied)
    std::string description = srv->description;
    if (auto it = body.find("description"); it != body.end() && it->value().is_string())
        description = std::string(it->value().as_string());

    std::string mdp_endpoint = srv->mdp_endpoint;
    if (auto it = body.find("mdp_endpoint"); it != body.end() && it->value().is_string())
        mdp_endpoint = std::string(it->value().as_string());

    std::vector<std::string> symbols = srv->active_symbols;
    if (auto it = body.find("active_symbols"); it != body.end() && it->value().is_array()) {
        symbols.clear();
        for (const auto& s : it->value().as_array()) {
            if (s.is_string()) symbols.emplace_back(std::string(s.as_string()));
        }
    }

    std::vector<std::string> allowlist;
    if (auto it = body.find("allowlist"); it != body.end() && it->value().is_array()) {
        for (const auto& s : it->value().as_array()) {
            if (s.is_string()) allowlist.emplace_back(std::string(s.as_string()));
        }
    } else {
        // Keep existing allowlist
        for (const auto& entry : ALLOWLIST) {
            if (entry.server_id == srv->server_id) {
                const auto* u = find_user_by_id(entry.user_id);
                if (u) allowlist.emplace_back(u->username);
            }
        }
    }

    // Validate all allowlist users exist
    for (const auto& uname : allowlist) {
        if (!find_user(uname)) {
            res["error"] = "User not found: " + uname;
            return res;
        }
    }

    // TODO: UPDATE servers SET description=$1, mdp_endpoint=$2 WHERE server_name=$3
    //       DELETE FROM server_symbols WHERE server_id=$4;
    //       INSERT INTO server_symbols (server_id, symbol) VALUES (...)
    //       DELETE FROM allowlist WHERE server_id=$4;
    //       INSERT INTO allowlist (server_id, user_id) VALUES (...)

    res["success"]      = true;
    res["server_name"]  = server_name;
    res["description"]  = description;
    res["mdp_endpoint"] = mdp_endpoint;

    json::array sym_arr;
    for (auto& s : symbols) sym_arr.emplace_back(s);
    res["active_symbols"] = std::move(sym_arr);

    json::array al_arr;
    for (auto& u : allowlist) al_arr.emplace_back(u);
    res["allowlist"] = std::move(al_arr);

    return res;
}

} // namespace backend

