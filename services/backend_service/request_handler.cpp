#include "request_handler.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <format>
#include <iostream>
#include <limits>
#include <ranges>
#include <thread>
#include <unordered_set>

namespace backend {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace {

constexpr std::size_t kServerNameMaxLength = 11;

bool is_server_name_valid(const std::string& server_name) {
    if (server_name.empty() || server_name.size() > kServerNameMaxLength) {
        return false;
    }
    return std::ranges::none_of(server_name, [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

} // namespace

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
        } else if (path == "/active_symbols") {
            res = handle_active_symbols(params);
        } else if (path == "/user_servers") {
            res = handle_user_servers(params);
        } else if (path == "/get_account_details") {
            res = handle_account_details(params);
        } else if (path == "/get_historical_trades") {
            res = handle_historical_trades(params);
        } else if (path == "/server_mdp_endpoint") {
            res = handle_server_mdp_endpoint(params);
            if (res.contains("error")) status = http::status::bad_request;
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
        } else if (path == "/remove_server") {
            res = handle_remove_server(req);
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

        auto mdp_endpoint_result = m_db_client.get_service_endpoint(srv.server_name, Service::mdp);
        if (!mdp_endpoint_result.has_value()) {
            std::cerr << mdp_endpoint_result.error() << '\n';
            res["error"] = "Internal server error";
            return res;
        }
        if (mdp_endpoint_result.value().has_value()) {
            const auto& endpoint = mdp_endpoint_result.value().value();
            obj["mdp_ip"] = endpoint.ip;
            obj["mdp_port"] = endpoint.port;
        } else {
            obj["mdp_ip"] = "";
            obj["mdp_port"] = 0;
        }

        bj::array symbols;
        for (const auto& s : srv.active_tickers) symbols.emplace_back(s);
        obj["active_symbols"] = std::move(symbols);

        arr.emplace_back(std::move(obj));
    }

    res["servers"] = std::move(arr);
    return res;
}

bj::object RequestHandler::handle_server_mdp_endpoint(const boost::urls::params_view& params) {
    bj::object res;

    auto server_name_it = params.find("server_name");
    if (server_name_it == params.end()) {
        res["error"] = "Missing server_name";
        return res;
    }

    const std::string server_name = (*server_name_it).value;

    auto endpoint_result = m_db_client.get_service_endpoint(server_name, Service::mdp);
    if (!endpoint_result.has_value()) {
        std::cerr << endpoint_result.error() << '\n';
        res["error"] = "Internal server error";
        return res;
    }
    if (!endpoint_result.value().has_value()) {
        res["error"] = "MDP endpoint not found";
        return res;
    }

    const auto& endpoint = endpoint_result.value().value();
    res["server_name"] = server_name;
    res["mdp_ip"] = endpoint.ip;
    res["mdp_port"] = endpoint.port;
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
    const int balance_multiplier = static_cast<int>(core::constants::decimal_to_int_multiplier);
    const int balance_multiplier_squared = balance_multiplier * balance_multiplier;
    
    for (const auto& srv : result.value()) {
        bj::object obj;
        obj["server_id"]   = srv.server_id;
        obj["server_name"] = srv.server_name;
        obj["role"]        = srv.role;
        obj["description"] = srv.description;
        obj["initial_usd"] = srv.initial_usd;

        bj::array symbols;
        for (const auto& s : srv.active_tickers) symbols.emplace_back(s);
        obj["active_symbols"] = std::move(symbols);

        bj::array bal_arr;
        for (const auto& b : srv.balances) {
            bj::object bo;
            bo["symbol"]  = b.symbol;
            const int divisor = (b.symbol == "USD") ? balance_multiplier_squared : balance_multiplier;
            bo["balance"] = static_cast<double>(b.balance) / divisor;
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

    const std::string server_name = (*server_it).value;
    const std::string symbol = (*symbol_it).value;

    auto after_ts_it = params.find("after_ts_ms");
    std::expected<std::vector<database::DatabaseClient::HistoricalTradeRow>, std::string> result;
    if (after_ts_it != params.end()) {
        long long after_ts_ms{};
        try { after_ts_ms = std::stoll(std::string((*after_ts_it).value)); }
        catch (...) { res["error"] = "Invalid after_ts_ms"; return res; }
        result = m_db_client.query_trades(server_name, symbol, after_ts_ms);
    } else {
        res["error"] = "No after_ts_ms specified";
        return res;
    }
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
    server_obj["initial_usd"] = details.initial_usd;

    bj::array ticker_arr;
    for (const auto& t : details.active_tickers) ticker_arr.emplace_back(t);
    server_obj["active_symbols"] = std::move(ticker_arr);
    res["server"] = std::move(server_obj);

    res["role"] = details.role;

    const int balance_multiplier = static_cast<int>(core::constants::decimal_to_int_multiplier);
    const int balance_multiplier_squared = balance_multiplier * balance_multiplier;
    
    bj::array bal_arr;
    double total_value = 0.0;
    for (const auto& b : details.balances) {
        bj::object bo;
        bo["symbol"]  = b.symbol;
        const int divisor = (b.symbol == "USD") ? balance_multiplier_squared : balance_multiplier;
        const double scaled_balance = static_cast<double>(b.balance) / divisor;
        bo["balance"] = scaled_balance;
        bal_arr.emplace_back(std::move(bo));
        total_value += scaled_balance;
    }
    res["balances"]    = std::move(bal_arr);
    res["total_value"] = total_value;

    const int initial_usd = details.initial_usd;
    res["initial_usd"] = initial_usd;
    res["pnl"]     = total_value - initial_usd;
    res["pnl_pct"] = initial_usd != 0
                         ? static_cast<double>(total_value - initial_usd) / initial_usd * 100.0
                         : 0.0;

    return res;
}

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
    if (!is_server_name_valid(server_name)) {
        res["error"] = "server_name must be less than 12 characters and contain no spaces";
        return res;
    }

    std::string description;
    if (auto it = body.find("description"); it != body.end() && it->value().is_string())
        description = std::string(it->value().as_string());

    int initial_usd = 100000;
    if (auto it = body.find("initial_usd"); it != body.end()) {
        if (!it->value().is_int64()) {
            res["error"] = "initial_usd must be an integer";
            return res;
        }
        const auto raw_initial_usd = it->value().as_int64();
        if (raw_initial_usd < 0 || raw_initial_usd > std::numeric_limits<int>::max()) {
            res["error"] = "initial_usd must be between 0 and 2147483647";
            return res;
        }
        initial_usd = static_cast<int>(raw_initial_usd);
    }

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
    if (symbols.empty()) {
        res["error"] = "active_symbols must contain at least one symbol";
        return res;
    }
    if (allowlist_names.empty()) {
        res["error"] = "allowlist must contain at least one user";
        return res;
    }

    // Resolve allowlist usernames to user IDs
    auto ids_result = m_db_client.resolve_user_ids(allowlist_names);
    if (!ids_result.has_value()) {
        res["error"] = ids_result.error();
        return res;
    }

    // TODO: implement machine selection when scaling to multiple machines.
    auto machines_res = m_db_client.query_machines();
    if (!machines_res.has_value()) {
        res["error"] = machines_res.error();
        return res;
    }
    auto machines = machines_res.value();
    if (machines.empty()) {
	res["error"] = "No available machines found";
	return res;
    }
    // use first available machine for now
    auto machine_id = machines[0].machine_id;
    auto machine_ip = machines[0].ip;
    auto machine_name = machines[0].machine_name;
    constexpr int kMinUnreservedPort = 10001;
    auto services_res = m_db_client.query_services(machine_name);
    if (!services_res.has_value()) {
        res["error"] = services_res.error();
        return res;
    }
    const auto services = services_res.value();
    std::unordered_set<int> used_ports;
    used_ports.reserve(services.size());
    for (const auto& service : services) {
        used_ports.insert(service.port);
    }

    int next_port_candidate = kMinUnreservedPort;
    auto take_next_available_port = [&]() {
        while (used_ports.contains(next_port_candidate)) {
            ++next_port_candidate;
        }
        const int assigned_port = next_port_candidate;
        used_ports.insert(assigned_port);
        ++next_port_candidate;
        return assigned_port;
    };

    int mdp_port = take_next_available_port();
    int me_port = take_next_available_port();
    int oms_port = take_next_available_port();
    int gateway_port = take_next_available_port();

    auto join_csv = [](const std::vector<std::string>& values) {
        std::string out;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out += ",";
            out += values[i];
        }
        return out;
    };

    int deployment_port = 10000; // assuming all deployment servers use port 10000
    auto deploy_service = [&](const std::string& service_name, const bj::object& params)
        -> std::expected<void, std::string> {
        try {
            net::io_context ioc;
            tcp::resolver resolver{ioc};
            beast::tcp_stream stream{ioc};
            stream.connect(resolver.resolve(machine_ip, std::to_string(deployment_port)));

            bj::object payload;
            payload["service_name"] = service_name;
            payload["server_name"] = server_name;
            payload["params"] = params;

            http::request<http::string_body> deploy_req{http::verb::post, "/deploy", 11};
            deploy_req.set(http::field::host, machine_ip);
            deploy_req.set(http::field::content_type, "application/json");
            deploy_req.set(http::field::connection, "close");
            deploy_req.body() = bj::serialize(payload);
            deploy_req.prepare_payload();

            http::write(stream, deploy_req);

            beast::flat_buffer buffer;
            http::response<http::string_body> deploy_res;
            http::read(stream, buffer, deploy_res);

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            if (deploy_res.result() != http::status::ok) {
                return std::unexpected{
                    std::format("deployment_server {}:{} returned status {} for {}: {}",
                        machine_ip, deployment_port, static_cast<unsigned>(deploy_res.result_int()),
                        service_name, deploy_res.body())
                };
            }

            bj::value parsed = bj::parse(deploy_res.body());
            if (!parsed.is_object()) {
                return std::unexpected{std::format(
                    "Invalid response body from deployment_server for {}: {}", service_name, deploy_res.body())};
            }
            const auto& obj = parsed.as_object();
            if (!obj.contains("status") || !obj.at("status").is_string() ||
                obj.at("status").as_string() != "deployed") {
                return std::unexpected{std::format(
                    "Deployment failed for {}: {}", service_name, deploy_res.body())};
            }
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error calling deployment_server {}:{} for {}: {}",
                    machine_ip, deployment_port, service_name, e.what())};
        }
    };

    // Create server and allowlist in database BEFORE deploying services
    // This ensures OMS can initialize balances when it starts
    std::vector<database::DatabaseClient::ServiceInsertRow> service_rows;
    service_rows.push_back(database::DatabaseClient::ServiceInsertRow{machine_id, Service::mdp, mdp_port});
    service_rows.push_back(database::DatabaseClient::ServiceInsertRow{machine_id, Service::me, me_port});
    service_rows.push_back(database::DatabaseClient::ServiceInsertRow{machine_id, Service::oms, oms_port});
    service_rows.push_back(database::DatabaseClient::ServiceInsertRow{machine_id, Service::gateway, gateway_port});

    auto create_result = m_db_client.create_server_with_services(
        server_name, caller_id, description, symbols, ids_result.value(), service_rows, initial_usd);
    if (!create_result.has_value()) {
        res["error"] = create_result.error();
        return res;
    }
    int server_id = create_result.value();

    // Now deploy the services - OMS can now read the server and allowlist from DB
    bj::object mdp_params;
    mdp_params["host"] = machine_ip;
    mdp_params["ws_port"] = mdp_port;
    mdp_params["active_symbols"] = join_csv(symbols);
    auto mdp_deploy_result = deploy_service("mdp", mdp_params);
    if (!mdp_deploy_result.has_value()) {
        res["error"] = mdp_deploy_result.error();
        return res;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    bj::object me_params;
    me_params["matching_engine_host"] = machine_ip;
    me_params["matching_engine_port"] = me_port;
    me_params["active_symbols"] = join_csv(symbols);
    auto me_deploy_result = deploy_service("me", me_params);
    if (!me_deploy_result.has_value()) {
        res["error"] = me_deploy_result.error();
        return res;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    bj::object oms_params;
    oms_params["order_manager_host"] = machine_ip;
    oms_params["order_manager_port"] = oms_port;
    oms_params["downstream_matching_engine_host"] = machine_ip;
    oms_params["downstream_matching_engine_port"] = me_port;

    auto oms_deploy_result = deploy_service("oms", oms_params);
    if (!oms_deploy_result.has_value()) {
        res["error"] = oms_deploy_result.error();
        return res;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    bj::object gateway_params;
    gateway_params["downstream_order_manager_host"] = machine_ip;
    gateway_params["downstream_order_manager_port"] = oms_port;
    gateway_params["active_symbols"] = join_csv(symbols);
    gateway_params["fix_server_port"] = gateway_port;
    gateway_params["whitelist"] = join_csv(allowlist_names);
    auto gateway_deploy_result = deploy_service("gateway", gateway_params);
    if (!gateway_deploy_result.has_value()) {
        res["error"] = gateway_deploy_result.error();
        return res;
    }
    auto create_table_res = m_db_client.create_quest_tables(server_name);
    if (!create_table_res.has_value()) {
        res["error"] = create_table_res.error();
        return res;
    }

    res["success"]    = true;
    res["server_id"]  = server_id;
    res["server_name"] = server_name;
    res["admin_id"]   = caller_id;
    res["description"] = description;
    res["initial_usd"] = initial_usd;

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

// ---------------------------------------------------------------------------
// POST /remove_server
// ---------------------------------------------------------------------------
bj::object RequestHandler::handle_remove_server(const http::request<http::string_body>& req) {
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

    constexpr int deployment_port = 10000;
    auto remove_service =
        [&](const std::string& service_type, Service service_enum) -> std::expected<void, std::string> {
        auto deployment_endpoint_result = m_db_client.get_service_endpoint(server_name, service_enum);
        if (!deployment_endpoint_result.has_value()) {
            return std::unexpected{deployment_endpoint_result.error()};
        }
        if (!deployment_endpoint_result.value().has_value()) {
            return std::unexpected{std::format(
                "Service endpoint not found for {} on server {}", service_type, server_name)};
        }
        const auto& endpoint = deployment_endpoint_result.value().value();

        try {
            net::io_context ioc;
            tcp::resolver resolver{ioc};
            beast::tcp_stream stream{ioc};
            stream.connect(resolver.resolve(endpoint.ip, std::to_string(deployment_port)));

            bj::object payload;
            payload["service_type"] = service_type;
            payload["server_name"] = server_name;

            http::request<http::string_body> remove_req{http::verb::post, "/remove_server", 11};
            remove_req.set(http::field::host, endpoint.ip);
            remove_req.set(http::field::content_type, "application/json");
            remove_req.set(http::field::connection, "close");
            remove_req.body() = bj::serialize(payload);
            remove_req.prepare_payload();

            http::write(stream, remove_req);

            beast::flat_buffer buffer;
            http::response<http::string_body> remove_res;
            http::read(stream, buffer, remove_res);

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            if (remove_res.result() != http::status::ok) {
                return std::unexpected{
                    std::format("deployment_server {}:{} returned status {} for remove {}: {}",
                        endpoint.ip, deployment_port, static_cast<unsigned>(remove_res.result_int()),
                        service_type, remove_res.body())
                };
            }

            bj::value parsed = bj::parse(remove_res.body());
            if (!parsed.is_object()) {
                return std::unexpected{std::format(
                    "Invalid response body from deployment_server for remove {}: {}",
                    service_type, remove_res.body())};
            }
            const auto& obj = parsed.as_object();
            if (!obj.contains("status") || !obj.at("status").is_string() ||
                obj.at("status").as_string() != "removed") {
                return std::unexpected{std::format(
                    "Remove failed for {}: {}", service_type, remove_res.body())};
            }
            m_db_client.drop_quest_tables(server_name);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected{
                std::format("Error calling deployment_server {}:{} for remove {}: {}",
                    endpoint.ip, deployment_port, service_type, e.what())};
        }
    };

    auto mdp_remove_result = remove_service("mdp", Service::mdp);
    if (!mdp_remove_result.has_value()) {
        res["error"] = mdp_remove_result.error();
        return res;
    }

    auto me_remove_result = remove_service("me", Service::me);
    if (!me_remove_result.has_value()) {
        res["error"] = me_remove_result.error();
        return res;
    }

    auto oms_remove_result = remove_service("oms", Service::oms);
    if (!oms_remove_result.has_value()) {
        res["error"] = oms_remove_result.error();
        return res;
    }

    auto gateway_remove_result = remove_service("gateway", Service::gateway);
    if (!gateway_remove_result.has_value()) {
        res["error"] = gateway_remove_result.error();
        return res;
    }

    auto delete_result = m_db_client.delete_server(server_name);
    if (!delete_result.has_value()) {
        res["error"] = delete_result.error();
        return res;
    }
    if (!delete_result.value()) {
        res["error"] = "Server not found: " + server_name;
        return res;
    }

    res["success"] = true;
    res["server_name"] = server_name;
    return res;
}

} // namespace backend
