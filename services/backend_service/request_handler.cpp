#include "request_handler.h"
#include "queries.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace backend {

// ── Hardcoded demo data ────────────────────────────────────────────────────

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
    {1, "alice",   "password123"},
    {2, "bob",     "password456"},
    {3, "charlie", "password789"},
};

static const std::vector<ServerRecord> SERVERS = {
    {1, "hk01",  1, {"AAPL", "GOOGL", "MSFT", "AMZN"}, "ws://localhost:9001"},
    {2, "us01",  2, {"TSLA", "META", "NVDA", "JPM"},    "ws://localhost:9002"},
    {3, "eu01",  1, {"AAPL", "TSLA"},                    "ws://localhost:9003"},
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

// ── Helper lookups ─────────────────────────────────────────────────────────

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

// ── Request handling ───────────────────────────────────────────────────────

http::response<http::string_body> RequestHandler::handle(const http::request<http::string_body>& req) {
    // Handle CORS preflight
    if (req.method() == http::verb::options) {
        http::response<http::string_body> response{http::status::no_content, req.version()};
        response.set(http::field::access_control_allow_origin, "*");
        response.set(http::field::access_control_allow_methods, "GET, OPTIONS");
        response.set(http::field::access_control_allow_headers, "Content-Type");
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
        } else if (path == "/get_historical_trades") {
            res = handle_historical_trades(params);
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
    //       return error if username already exists
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

} // namespace backend

