// Integration-style test: spawns the backend_service process, seeds the
// PostgreSQL database with known data, and exercises every HTTP endpoint by
// sending real HTTP requests with Boost.Beast.  GTest assertions verify that
// responses match the expected Jserver_idSON shape and values.
//
// Prerequisites:
//   - PostgreSQL running at localhost:5432 with the edux_core_db database
//     initialised (scripts/init_databases.sh).
//   - BACKEND_SERVICE_BINARY must be defined via CMake (auto-injected).

#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <pqxx/pqxx>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef BACKEND_SERVICE_BINARY
#  error "BACKEND_SERVICE_BINARY must be defined via CMake target_compile_definitions"
#endif

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace json  = boost::json;
using tcp       = net::ip::tcp;

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int         TEST_PORT    = 18080;
static constexpr const char* TEST_HOST    = "127.0.0.1";
static constexpr const char* DB_CONN      = "host=localhost port=5432 dbname=edux_core_db";
static constexpr const char* TOML_PATH    = "/tmp/test_backend_service.toml";

// ── HTTP helpers ──────────────────────────────────────────────────────────────

struct Response {
    http::status status;
    json::value  body;
};

static Response do_request(http::verb          verb,
                            const std::string&  target,
                            const std::string&  auth     = "",
                            const std::string&  body_str = "") {
    net::io_context   ioc;
    tcp::resolver     resolver{ioc};
    beast::tcp_stream stream{ioc};

    stream.connect(resolver.resolve(TEST_HOST, std::to_string(TEST_PORT)));

    http::request<http::string_body> req{verb, target, 11};
    req.set(http::field::host,       TEST_HOST);
    req.set(http::field::connection, "close");
    if (!auth.empty())
        req.set(http::field::authorization, "Bearer " + auth);
    if (!body_str.empty()) {
        req.set(http::field::content_type, "application/json");
        req.body() = body_str;
        req.prepare_payload();
    }

    http::write(stream, req);

    beast::flat_buffer               buf;
    http::response<http::string_body> res;
    http::read(stream, buf, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    return {res.result(), res.body().empty() ? json::value{} : json::parse(res.body())};
}

static Response do_get(const std::string& target) {
    return do_request(http::verb::get, target);
}

static Response do_post(const std::string& target,
                         const json::object& body,
                         const std::string&  auth = "") {
    return do_request(http::verb::post, target, auth, json::serialize(body));
}

// ── Process management ────────────────────────────────────────────────────────

static pid_t g_backend_pid = -1;

static bool wait_for_port(int port, int timeout_ms = 8000) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        try {
            net::io_context   ioc;
            tcp::resolver     resolver{ioc};
            beast::tcp_stream stream{ioc};
            stream.connect(resolver.resolve(TEST_HOST, std::to_string(port)));
            stream.socket().close();
            return true;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return false;
}

static void start_backend() {
    {
        std::ofstream f{TOML_PATH};
        if (!f)
            throw std::runtime_error(std::string{"Cannot write test config to "} + TOML_PATH);
        f << "host = \"" << TEST_HOST << "\"\n";
        f << "port = " << TEST_PORT << "\n";
        f << "uat = false\n";
    }

    g_backend_pid = fork();
    if (g_backend_pid < 0)
        throw std::runtime_error("fork() failed");

    if (g_backend_pid == 0) {
        // child process
        execl(BACKEND_SERVICE_BINARY, BACKEND_SERVICE_BINARY, TOML_PATH, nullptr);
        std::perror("execl");
        _exit(1);
    }

    // parent: wait until the port is open
    if (!wait_for_port(TEST_PORT))
        throw std::runtime_error(
            "backend_service failed to start on port " + std::to_string(TEST_PORT));
}

static void cleanup_all_test_db_rows() {
    try {
        pqxx::connection conn{DB_CONN};
        pqxx::work txn{conn};
        txn.exec(
            "DELETE FROM allowlist "
            "WHERE server_id IN "
            "  (SELECT server_id FROM servers WHERE server_name LIKE 'test_%')");
        txn.exec(
            "DELETE FROM balances "
            "WHERE user_id IN "
            "  (SELECT user_id FROM users WHERE username LIKE 'test_%')");
        txn.exec("DELETE FROM servers WHERE server_name LIKE 'test_%'");
        txn.exec("DELETE FROM users   WHERE username    LIKE 'test_%'");
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "Warning: global DB cleanup failed: " << e.what() << '\n';
    }
}

static void stop_backend() {
    if (g_backend_pid > 0) {
        kill(g_backend_pid, SIGTERM);
        int status = 0;
        waitpid(g_backend_pid, &status, 0);
        g_backend_pid = -1;
    }
    std::filesystem::remove(TOML_PATH);
}

// ── Global test environment (process lifecycle + final DB cleanup) ────────────

class BackendServiceEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        cleanup_all_test_db_rows();
        start_backend();
    }
    void TearDown() override {
        stop_backend();
        cleanup_all_test_db_rows();
    }
};

// ── Per-test fixture (database seeding) ──────────────────────────────────────

class BackendServiceTest : public ::testing::Test {
protected:
    // IDs of rows created by seed(); available to every test case.
    int m_admin_id  = -1;
    int m_member_id = -1;
    int m_server_id = -1;

    void SetUp() override {
        pqxx::connection conn{DB_CONN};
        cleanup(conn);
        seed(conn);
    }

    void TearDown() override {
        pqxx::connection conn{DB_CONN};
        cleanup(conn);
    }

private:
    // Remove all rows whose names start with "test_" to isolate tests from
    // production data and from each other.
    static void cleanup(pqxx::connection& conn) {
        pqxx::work txn{conn};
        txn.exec(
            "DELETE FROM allowlist "
            "WHERE server_id IN "
            "  (SELECT server_id FROM servers WHERE server_name LIKE 'test_%')");
        txn.exec(
            "DELETE FROM balances "
            "WHERE user_id IN "
            "  (SELECT user_id FROM users WHERE username LIKE 'test_%')");
        txn.exec("DELETE FROM servers WHERE server_name LIKE 'test_%'");
        txn.exec("DELETE FROM users   WHERE username    LIKE 'test_%'");
        txn.commit();
    }

    // Fixed IDs used for all test rows.  They are far above any real data
    // and are scrubbed by cleanup() before and after every test.
    static constexpr int ADMIN_ID  = 9001;
    static constexpr int MEMBER_ID = 9002;
    static constexpr int SERVER_ID = 9001;

    void seed(pqxx::connection& conn) {
        pqxx::work txn{conn};

        // Users — user_id has no sequence so we supply explicit IDs.
        txn.exec(
            "INSERT INTO users (user_id, username, password) "
            "VALUES ($1, 'test_admin', 'adminpass')",
            pqxx::params{ADMIN_ID});
        m_admin_id = ADMIN_ID;

        txn.exec(
            "INSERT INTO users (user_id, username, password) "
            "VALUES ($1, 'test_member', 'memberpass')",
            pqxx::params{MEMBER_ID});
        m_member_id = MEMBER_ID;

        // Server owned by test_admin — server_id also has no sequence.
        txn.exec(
            "INSERT INTO servers (server_id, server_name, admin_id, active_tickers, description) "
            "VALUES ($1, 'test_server', $2, ARRAY['AAPL','GOOG']::varchar[], 'Test trading server')",
            pqxx::params{SERVER_ID, m_admin_id});
        m_server_id = SERVER_ID;

        // test_member is on the allowlist
        txn.exec(
            "INSERT INTO allowlist (server_id, user_id) VALUES ($1, $2)",
            pqxx::params{m_server_id, m_member_id});

        // Balances for test_member (PK is (user_id, symbol))
        txn.exec(
            "INSERT INTO balances (user_id, server_id, symbol, balance) "
            "VALUES ($1, $2, 'USD', 100000)",
            pqxx::params{m_member_id, m_server_id});
        txn.exec(
            "INSERT INTO balances (user_id, server_id, symbol, balance) "
            "VALUES ($1, $2, 'AAPL', 50)",
            pqxx::params{m_member_id, m_server_id});

        txn.commit();
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Tests
// ═════════════════════════════════════════════════════════════════════════════

// ── GET /login ────────────────────────────────────────────────────────────────

TEST_F(BackendServiceTest, LoginSuccess) {
    auto r = do_get("/login?user_name=test_admin&password=adminpass");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().at("success").as_bool());
    EXPECT_EQ(r.body.as_object().at("err_msg").as_string(), "");
}

TEST_F(BackendServiceTest, LoginWrongPassword) {
    auto r = do_get("/login?user_name=test_admin&password=wrongpass");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_FALSE(r.body.as_object().at("success").as_bool());
    EXPECT_NE(r.body.as_object().at("err_msg").as_string(), "");
}

TEST_F(BackendServiceTest, LoginMissingParams) {
    auto r = do_get("/login?user_name=test_admin");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_FALSE(r.body.as_object().at("success").as_bool());
}

// ── GET /signup ───────────────────────────────────────────────────────────────

// NOTE: The users table has no SERIAL/sequence on user_id, so the backend's
// create_user INSERT (which omits user_id) violates the NOT NULL constraint.
TEST_F(BackendServiceTest, SignupNewUser) {
    auto r = do_get("/signup?user_name=test_newuser&password=newpass");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_EQ(r.body.as_object().at("success").as_bool(), true);
}

TEST_F(BackendServiceTest, SignupDuplicateUser) {
    auto r = do_get("/signup?user_name=test_admin&password=adminpass");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_FALSE(r.body.as_object().at("success").as_bool());
    EXPECT_NE(r.body.as_object().at("err_msg").as_string(), "");
}

TEST_F(BackendServiceTest, SignupMissingParams) {
    auto r = do_get("/signup?user_name=test_nopass");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_FALSE(r.body.as_object().at("success").as_bool());
}

// ── GET /active_servers ───────────────────────────────────────────────────────

TEST_F(BackendServiceTest, ActiveServersContainsSeededServer) {
    auto r = do_get("/active_servers");
    EXPECT_EQ(r.status, http::status::ok);

    bool found = false;
    for (const auto& s : r.body.as_object().at("servers").as_array()) {
        const auto& obj = s.as_object();
        if (obj.at("server_name").as_string() == "test_server") {
            EXPECT_EQ(obj.at("admin_name").as_string(),  "test_admin");
            EXPECT_EQ(obj.at("description").as_string(), "Test trading server");
            EXPECT_EQ(obj.at("active_symbols").as_array().size(), 2u);
            found = true;
        }
    }
    EXPECT_TRUE(found) << "test_server was not returned by /active_servers";
}

// ── GET /user_info ────────────────────────────────────────────────────────────

TEST_F(BackendServiceTest, UserInfoReturnsUserAndBalances) {
    auto r = do_get("/user_info?user_name=test_member");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& obj = r.body.as_object();
    EXPECT_EQ(obj.at("user").as_object().at("username").as_string(), "test_member");
    // Two balances seeded: USD and AAPL
    EXPECT_EQ(obj.at("balances").as_array().size(), 2u);
}

TEST_F(BackendServiceTest, UserInfoMissingParam) {
    auto r = do_get("/user_info");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

TEST_F(BackendServiceTest, UserInfoUnknownUser) {
    auto r = do_get("/user_info?user_name=no_such_user_xyz");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── GET /active_symbols ───────────────────────────────────────────────────────

TEST_F(BackendServiceTest, ActiveSymbolsForSeededServer) {
    auto r = do_get("/active_symbols?server_name=test_server");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& syms = r.body.as_object().at("symbols").as_array();
    EXPECT_EQ(syms.size(), 2u);

    std::vector<std::string> got;
    for (const auto& s : syms)
        got.emplace_back(s.as_string());
    EXPECT_TRUE((got == std::vector<std::string>{"AAPL", "GOOG"}) ||
                (got == std::vector<std::string>{"GOOG", "AAPL"}));
}

TEST_F(BackendServiceTest, ActiveSymbolsMissingParam) {
    auto r = do_get("/active_symbols");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── GET /user_servers ─────────────────────────────────────────────────────────

TEST_F(BackendServiceTest, UserServersForAdmin) {
    auto r = do_get("/user_servers?user_name=test_admin");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& arr = r.body.as_object().at("servers").as_array();
    ASSERT_EQ(arr.size(), 1u);
    EXPECT_EQ(arr[0].as_object().at("server_name").as_string(), "test_server");
    EXPECT_EQ(arr[0].as_object().at("role").as_string(),        "admin");
}

TEST_F(BackendServiceTest, UserServersForMember) {
    auto r = do_get("/user_servers?user_name=test_member");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& arr = r.body.as_object().at("servers").as_array();
    ASSERT_EQ(arr.size(), 1u);
    EXPECT_EQ(arr[0].as_object().at("server_name").as_string(), "test_server");
    EXPECT_EQ(arr[0].as_object().at("role").as_string(),        "member");
    // Balances should be embedded
    EXPECT_EQ(arr[0].as_object().at("balances").as_array().size(), 2u);
}

TEST_F(BackendServiceTest, UserServersMissingParam) {
    auto r = do_get("/user_servers");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── GET /get_account_details ──────────────────────────────────────────────────

TEST_F(BackendServiceTest, AccountDetailsMemberAccess) {
    auto r = do_get("/get_account_details?user_name=test_member&server_name=test_server");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& obj = r.body.as_object();
    EXPECT_EQ(obj.at("role").as_string(), "member");
    EXPECT_EQ(obj.at("server").as_object().at("server_name").as_string(), "test_server");
    // USD=100000 + AAPL=50
    EXPECT_EQ(obj.at("balances").as_array().size(), 2u);
    EXPECT_EQ(obj.at("total_value").as_int64(), 100050);
    EXPECT_EQ(obj.at("pnl").as_int64(),         50);
}

TEST_F(BackendServiceTest, AccountDetailsAdminAccess) {
    auto r = do_get("/get_account_details?user_name=test_admin&server_name=test_server");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_EQ(r.body.as_object().at("role").as_string(), "admin");
}

TEST_F(BackendServiceTest, AccountDetailsNonMember) {
    // Create a user that is not on the server's allowlist
    {
        pqxx::connection conn{DB_CONN};
        pqxx::work txn{conn};
        txn.exec(
            "INSERT INTO users (user_id, username, password) VALUES (9003, 'test_outsider', 'pass')");
        txn.commit();
    }
    auto r = do_get(
        "/get_account_details?user_name=test_outsider&server_name=test_server");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

TEST_F(BackendServiceTest, AccountDetailsMissingParams) {
    auto r = do_get("/get_account_details?user_name=test_member");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── GET /get_historical_trades ────────────────────────────────────────────────

TEST_F(BackendServiceTest, HistoricalTradesResponseIsStructurallyValid) {
    // QuestDB may not be running in every test environment; accept either a
    // valid trades array or an error string — both are structurally correct.
    auto r = do_get("/get_historical_trades?server=test_server&symbol=AAPL");
    EXPECT_EQ(r.status, http::status::ok);
    const auto& obj = r.body.as_object();
    EXPECT_TRUE(obj.contains("trades") || obj.contains("error"));
    if (obj.contains("trades")) {
        EXPECT_TRUE(obj.at("trades").is_array());
    }
}

TEST_F(BackendServiceTest, HistoricalTradesMissingServerParam) {
    auto r = do_get("/get_historical_trades?symbol=AAPL");
    EXPECT_EQ(r.status, http::status::ok);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── POST /create_server ───────────────────────────────────────────────────────

TEST_F(BackendServiceTest, CreateServerSuccess) {
    json::object body;
    body["server_name"]    = "test_new_server";
    body["description"]    = "Created in test";
    body["active_symbols"] = json::array{"TSLA"};
    body["allowlist"]      = json::array{"test_member"};

    auto r = do_post("/create_server", body, "test_admin");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& obj = r.body.as_object();
    EXPECT_TRUE(obj.at("success").as_bool());
    EXPECT_EQ(obj.at("server_name").as_string(),  "test_new_server");
    EXPECT_EQ(obj.at("description").as_string(),  "Created in test");
    ASSERT_EQ(obj.at("active_symbols").as_array().size(), 1u);
    EXPECT_EQ(obj.at("active_symbols").as_array()[0].as_string(), "TSLA");
    ASSERT_EQ(obj.at("allowlist").as_array().size(), 1u);
    EXPECT_EQ(obj.at("allowlist").as_array()[0].as_string(), "test_member");
}

TEST_F(BackendServiceTest, CreateServerNoAuth) {
    json::object body;
    body["server_name"] = "test_unauth_server";

    auto r = do_post("/create_server", body);
    EXPECT_EQ(r.status, http::status::unauthorized);
    EXPECT_TRUE(r.body.as_object().contains("auth_error"));
}

TEST_F(BackendServiceTest, CreateServerMissingServerName) {
    json::object body;
    body["description"] = "No name given";

    auto r = do_post("/create_server", body, "test_admin");
    EXPECT_EQ(r.status, http::status::bad_request);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

TEST_F(BackendServiceTest, CreateServerUnknownAllowlistUser) {
    json::object body;
    body["server_name"] = "test_bad_allowlist_server";
    body["allowlist"]   = json::array{"no_such_user_xyz"};

    auto r = do_post("/create_server", body, "test_admin");
    EXPECT_EQ(r.status, http::status::bad_request);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

// ── POST /configure_server ────────────────────────────────────────────────────

TEST_F(BackendServiceTest, ConfigureServerByAdmin) {
    json::object body;
    body["server_name"]    = "test_server";
    body["description"]    = "Updated description";
    body["active_symbols"] = json::array{"MSFT", "AMZN"};
    body["allowlist"]      = json::array{};

    auto r = do_post("/configure_server", body, "test_admin");
    EXPECT_EQ(r.status, http::status::ok);

    const auto& obj = r.body.as_object();
    EXPECT_TRUE(obj.at("success").as_bool());
    EXPECT_EQ(obj.at("server_name").as_string(),  "test_server");
    EXPECT_EQ(obj.at("description").as_string(),  "Updated description");
    EXPECT_EQ(obj.at("active_symbols").as_array().size(), 2u);
}

TEST_F(BackendServiceTest, ConfigureServerByNonAdmin) {
    json::object body;
    body["server_name"] = "test_server";
    body["description"] = "Attempted by member";

    auto r = do_post("/configure_server", body, "test_member");
    EXPECT_EQ(r.status, http::status::unauthorized);
    EXPECT_TRUE(r.body.as_object().contains("auth_error"));
}

TEST_F(BackendServiceTest, ConfigureServerNotFound) {
    json::object body;
    body["server_name"] = "test_nonexistent_server";

    auto r = do_post("/configure_server", body, "test_admin");
    EXPECT_EQ(r.status, http::status::bad_request);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

TEST_F(BackendServiceTest, ConfigureServerNoAuth) {
    json::object body;
    body["server_name"] = "test_server";

    auto r = do_post("/configure_server", body);
    EXPECT_EQ(r.status, http::status::unauthorized);
    EXPECT_TRUE(r.body.as_object().contains("auth_error"));
}

// ── Unknown / method-not-allowed ──────────────────────────────────────────────

TEST_F(BackendServiceTest, UnknownEndpointReturns404) {
    auto r = do_get("/no_such_endpoint");
    EXPECT_EQ(r.status, http::status::not_found);
    EXPECT_TRUE(r.body.as_object().contains("error"));
}

TEST_F(BackendServiceTest, CorsPreflight) {
    auto r = do_request(http::verb::options, "/login");
    EXPECT_EQ(r.status, http::status::no_content);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BackendServiceEnvironment);
    return RUN_ALL_TESTS();
}
