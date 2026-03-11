#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <database/database_client.h>
#include <core/containers.h>
#include <core/orders.h>
#include <pqxx/pqxx>
#include <atomic>
#include <set>
#include <thread>
#include <chrono>
#include <unordered_map>

// HONEST ADMISSION: This was vibe-coded lol.
// NOTE: This clears the original database entries.

using namespace database;
using namespace core;

// ---------------------------------------------------------------------------
// Helper: small sleep so QuestDB has time to ingest rows via ILP before we
// query them back through the PG wire protocol.
// ---------------------------------------------------------------------------
static void wait_for_questdb_ingestion() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

// ---------------------------------------------------------------------------
// Connection string for the core PostgreSQL database (same as DatabaseClient).
// ---------------------------------------------------------------------------
static constexpr auto CORE_DB_CONN_STR =
    "host=localhost port=5432 dbname=edux_core_db";

// ---------------------------------------------------------------------------
// Test-specific user/balance constants.
// ---------------------------------------------------------------------------
static constexpr int         TEST_USER_ID           = 99999;
static constexpr int         TEST_NON_EXIST_USER_ID = 67676767;
static constexpr int         TEST_SERVER_ID         = 1;
static constexpr const char* TEST_USERNAME       = "test_db_user_99999";
static constexpr const char* TEST_SERVER_NAME    = "test_server_1";
static constexpr const char* TEST_PASSWORD       = "test_password";
static constexpr const char* TEST_SYMBOL         = "USD";
static constexpr int         TEST_INITIAL_BALANCE = 5000;

// ===========================================================================
//  Fixture: seeds a test user + balance row, tears down after each section.
// ===========================================================================
struct BalanceFixture {
    pqxx::connection conn{CORE_DB_CONN_STR};
    DatabaseClient   db;

    BalanceFixture() {
        // Ensure clean state: remove any leftover rows from a prior crashed run.
        cleanup_quietly();

        pqxx::work txn{conn};
        txn.exec(
            "INSERT INTO users (user_id, username, password, created_ts, last_modified_ts) "
            "VALUES ($1, $2, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, TEST_USERNAME, TEST_PASSWORD});
        txn.exec(
            "INSERT INTO servers (server_id, server_name) VALUES ($1, $2)",
            pqxx::params{TEST_SERVER_ID, TEST_SERVER_NAME});
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance, server_id, created_ts, modified_ts) "
            "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, TEST_SYMBOL, TEST_INITIAL_BALANCE, TEST_SERVER_ID});
        txn.commit();
    }

    ~BalanceFixture() {
        cleanup_quietly();
    }

  private:
    void cleanup_quietly() {
        try {
            pqxx::work txn{conn};
            txn.exec("DELETE FROM balances WHERE user_id = $1",
                     pqxx::params{TEST_USER_ID});
            txn.exec("DELETE FROM servers WHERE server_id = $1",
                     pqxx::params{TEST_SERVER_ID});
            txn.exec("DELETE FROM users WHERE user_id = $1",
                     pqxx::params{TEST_USER_ID});
            txn.commit();
        } catch (...) {
            // Best-effort cleanup; swallow errors.
        }
    }
};

// ===========================================================================
//  read_balance / update_balance  (PostgreSQL – edux_core_db)
// ===========================================================================

TEST_CASE_METHOD(BalanceFixture,
                 "read_balance returns the seeded value for the test user",
                 "[DatabaseClient][balance]") {
    auto result = db.read_balance(TEST_USER_ID, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == TEST_INITIAL_BALANCE);
}

TEST_CASE_METHOD(BalanceFixture,
                 "read_balance returns error for non-existing user",
                 "[DatabaseClient][balance]") {
    auto result = db.read_balance(TEST_NON_EXIST_USER_ID, TEST_SERVER_ID, TEST_SYMBOL);
    // No row should match, so the query_value call throws -> std::unexpected.
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(BalanceFixture,
                 "update_balance changes the balance and read_balance reflects it",
                 "[DatabaseClient][balance]") {
    const int new_balance = TEST_INITIAL_BALANCE + 100;

    auto update_result = db.update_balance(
        TEST_USER_ID, TEST_SERVER_ID, TEST_SYMBOL, new_balance);
    REQUIRE(update_result.has_value());

    auto after_update = db.read_balance(
        TEST_USER_ID, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(after_update.has_value());
    REQUIRE(after_update.value() == new_balance);
}

// ===========================================================================
//  insert_order  (QuestDB – orders table via ILP)
// ===========================================================================

TEST_CASE("insert_order succeeds and row is queryable", "[DatabaseClient][orders]") {
    DatabaseClient db;

    // Clean slate.
    auto truncate_result = db.truncate_orders();
    REQUIRE(truncate_result.has_value());
    wait_for_questdb_ingestion();

    NewOrderSingleContainer order{
        .sender_comp_id = "TEST_CLIENT",
        .target_comp_id = "EXCHANGE",
        .cl_ord_id      = "1001",
        .symbol         = "AAPL",
        .side           = Side::bid,
        .order_qty      = 50,
        .ord_type       = OrderType::limit,
        .price          = 15000, // represents $150.00 in internal format
        .time_in_force  = TimeInForce::day,
    };

    auto result = db.insert_order(1, order);
    REQUIRE(result.has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= 1);

    // Find our row.
    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 1 && r.cl_order_id == 1001) {
            CHECK(r.symbol         == "AAPL");
            CHECK(r.side           == "BID");
            CHECK(r.order_qty      == 50);
            CHECK(r.filled_qty     == 0);
            CHECK(r.ord_type       == "LIMIT");
            CHECK(r.price          == 15000);
            CHECK(r.time_in_force  == "DAY");
            CHECK(r.order_status   == "NEW");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    // Cleanup.
    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

// ===========================================================================
//  insert_cancel  (QuestDB – orders table via ILP, status CANCELED)
// ===========================================================================

TEST_CASE("insert_cancel succeeds and row is queryable", "[DatabaseClient][orders]") {
    DatabaseClient db;

    auto truncate_result = db.truncate_orders();
    REQUIRE(truncate_result.has_value());
    wait_for_questdb_ingestion();

    CancelOrderRequestContainer cancel{
        .sender_comp_id = "TEST_CLIENT",
        .target_comp_id = "EXCHANGE",
        .order_id       = "42",
        .orig_cl_ord_id = "1001",
        .cl_ord_id      = "1002",
        .symbol         = "AAPL",
        .side           = Side::ask,
        .order_qty      = 25,
    };

    auto result = db.insert_cancel(cancel);
    REQUIRE(result.has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= 1);

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 42 && r.cl_order_id == 1002) {
            CHECK(r.symbol       == "AAPL");
            CHECK(r.side         == "ASK");
            CHECK(r.order_qty    == 25);
            CHECK(r.filled_qty   == 0);
            CHECK(r.order_status == "CANCELED");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

// ===========================================================================
//  COMPREHENSIVE TESTS – balance edge cases
// ===========================================================================

// Fixture that seeds the test user with two symbols (USD and AAPL).
struct MultiSymbolBalanceFixture {
    pqxx::connection conn{CORE_DB_CONN_STR};
    DatabaseClient   db;

    static constexpr int AAPL_INITIAL_BALANCE = 200;

    MultiSymbolBalanceFixture() {
        cleanup_quietly();

        pqxx::work txn{conn};
        txn.exec(
            "INSERT INTO users (user_id, username, password, created_ts, last_modified_ts) "
            "VALUES ($1, $2, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, TEST_USERNAME, TEST_PASSWORD});
        txn.exec(
            "INSERT INTO servers (server_id, server_name) VALUES ($1, $2)",
            pqxx::params{TEST_SERVER_ID, TEST_SERVER_NAME});
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance, server_id, created_ts, modified_ts) "
            "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, TEST_SYMBOL, TEST_INITIAL_BALANCE, TEST_SERVER_ID});
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance, server_id, created_ts, modified_ts) "
            "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, "AAPL", AAPL_INITIAL_BALANCE, TEST_SERVER_ID});
        txn.commit();
    }

    ~MultiSymbolBalanceFixture() {
        cleanup_quietly();
    }

  private:
    void cleanup_quietly() {
        try {
            pqxx::work txn{conn};
            txn.exec("DELETE FROM balances WHERE user_id = $1",
                     pqxx::params{TEST_USER_ID});
            txn.exec("DELETE FROM servers WHERE server_id = $1",
                     pqxx::params{TEST_SERVER_ID});
            txn.exec("DELETE FROM users WHERE user_id = $1",
                     pqxx::params{TEST_USER_ID});
            txn.commit();
        } catch (...) {}
    }
};

TEST_CASE_METHOD(MultiSymbolBalanceFixture,
                 "read_balance distinguishes between symbols for the same user",
                 "[DatabaseClient][balance][comprehensive]") {
    auto usd = db.read_balance(TEST_USER_ID, TEST_SERVER_ID, TEST_SYMBOL);
    auto aapl = db.read_balance(TEST_USER_ID, TEST_SERVER_ID, "AAPL");

    REQUIRE(usd.has_value());
    REQUIRE(aapl.has_value());
    REQUIRE(usd.value() == TEST_INITIAL_BALANCE);
    REQUIRE(aapl.value() == AAPL_INITIAL_BALANCE);
}

TEST_CASE_METHOD(MultiSymbolBalanceFixture,
                 "read_balance returns error for existing user but wrong symbol",
                 "[DatabaseClient][balance][comprehensive]") {
    auto result = db.read_balance(TEST_USER_ID, TEST_SERVER_ID, "NONEXISTENT_SYM");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(MultiSymbolBalanceFixture,
                 "update_balance on one symbol does not affect another",
                 "[DatabaseClient][balance][comprehensive]") {
    const auto uid = TEST_USER_ID;

    auto update = db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 9999);
    REQUIRE(update.has_value());

    auto usd = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
    auto aapl = db.read_balance(uid, TEST_SERVER_ID, "AAPL");
    REQUIRE(usd.has_value());
    REQUIRE(aapl.has_value());
    CHECK(usd.value() == 9999);
    CHECK(aapl.value() == AAPL_INITIAL_BALANCE); // untouched
}

TEST_CASE_METHOD(BalanceFixture,
                 "update_balance to zero is valid",
                 "[DatabaseClient][balance][comprehensive]") {
    const auto uid = TEST_USER_ID;

    auto update = db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 0);
    REQUIRE(update.has_value());

    auto result = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0);
}

TEST_CASE_METHOD(BalanceFixture,
                 "update_balance is idempotent – same value twice yields same result",
                 "[DatabaseClient][balance][comprehensive]") {
    const auto uid = TEST_USER_ID;
    const int val = 4242;

    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, val).has_value());
    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, val).has_value());

    auto result = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == val);
}

TEST_CASE_METHOD(BalanceFixture,
                 "update_balance multiple times reads back the last written value",
                 "[DatabaseClient][balance][comprehensive]") {
    const auto uid = TEST_USER_ID;

    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 100).has_value());
    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 200).has_value());
    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 300).has_value());

    auto result = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 300);
}

// ===========================================================================
//  read_balances  (returns all symbol/balance rows for a user)
// ===========================================================================

TEST_CASE_METHOD(BalanceFixture,
                 "read_balances returns a single entry for a single-symbol user",
                 "[DatabaseClient][balance][read_balances]") {
    const auto uid = TEST_USER_ID;

    auto result = db.read_balances(uid, TEST_SERVER_ID);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    CHECK(result->at(0).symbol  == TEST_SYMBOL);
    CHECK(result->at(0).balance == TEST_INITIAL_BALANCE);
}

TEST_CASE_METHOD(MultiSymbolBalanceFixture,
                 "read_balances returns all symbols for a multi-symbol user",
                 "[DatabaseClient][balance][read_balances]") {
    const auto uid = TEST_USER_ID;

    auto result = db.read_balances(uid, TEST_SERVER_ID);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);

    // Collect into a map for order-independent comparison.
    std::unordered_map<std::string, int> by_symbol;
    for (const auto& row : result.value())
        by_symbol[row.symbol] = row.balance;

    REQUIRE(by_symbol.count(TEST_SYMBOL) == 1);
    REQUIRE(by_symbol.count("AAPL") == 1);
    CHECK(by_symbol.at(TEST_SYMBOL) == TEST_INITIAL_BALANCE);
    CHECK(by_symbol.at("AAPL")      == AAPL_INITIAL_BALANCE);
}

TEST_CASE_METHOD(BalanceFixture,
                 "read_balances returns empty vector for non-existing user",
                 "[DatabaseClient][balance][read_balances]") {
    auto result = db.read_balances(TEST_NON_EXIST_USER_ID, TEST_SERVER_ID);
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

TEST_CASE_METHOD(MultiSymbolBalanceFixture,
                 "read_balances reflects updated balance after update_balance",
                 "[DatabaseClient][balance][read_balances]") {
    const auto uid = TEST_USER_ID;
    const int new_usd = 9876;

    REQUIRE(db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, new_usd).has_value());

    auto result = db.read_balances(uid, TEST_SERVER_ID);
    REQUIRE(result.has_value());

    std::unordered_map<std::string, int> by_symbol;
    for (const auto& row : result.value())
        by_symbol[row.symbol] = row.balance;

    CHECK(by_symbol.at(TEST_SYMBOL) == new_usd);
    CHECK(by_symbol.at("AAPL")      == AAPL_INITIAL_BALANCE);
}

// ===========================================================================
//  COMPREHENSIVE TESTS – insert_order edge cases
// ===========================================================================

TEST_CASE("insert_order market order stores price as 0",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    NewOrderSingleContainer market_order{
        .sender_comp_id = "MARKET_CLIENT",
        .target_comp_id = "EXCHANGE",
        .cl_ord_id      = "5001",
        .symbol         = "TSLA",
        .side           = Side::ask,
        .order_qty      = 10,
        .ord_type       = OrderType::market,
        .price          = std::nullopt, // no price for market orders
        .time_in_force  = TimeInForce::day,
    };

    auto result = db.insert_order(500, market_order);
    REQUIRE(result.has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 500 && r.cl_order_id == 5001) {
            CHECK(r.symbol        == "TSLA");
            CHECK(r.side          == "ASK");
            CHECK(r.ord_type      == "MARKET");
            CHECK(r.price         == 0);       // nullopt -> 0
            CHECK(r.order_qty     == 10);
            CHECK(r.filled_qty    == 0);
            CHECK(r.order_status  == "NEW");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

TEST_CASE("insert_order multiple orders are all queryable",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    // Insert three distinct orders.
    NewOrderSingleContainer order_a{
        .sender_comp_id = "CLIENT_A",  .target_comp_id = "EXCHANGE",
        .cl_ord_id = "3001", .symbol = "AAPL", .side = Side::bid,
        .order_qty = 10, .ord_type = OrderType::limit,
        .price = 15000, .time_in_force = TimeInForce::day,
    };
    NewOrderSingleContainer order_b{
        .sender_comp_id = "CLIENT_B",  .target_comp_id = "EXCHANGE",
        .cl_ord_id = "3002", .symbol = "GOOG", .side = Side::ask,
        .order_qty = 20, .ord_type = OrderType::limit,
        .price = 28000, .time_in_force = TimeInForce::gtc,
    };
    NewOrderSingleContainer order_c{
        .sender_comp_id = "CLIENT_A",  .target_comp_id = "EXCHANGE",
        .cl_ord_id = "3003", .symbol = "AAPL", .side = Side::ask,
        .order_qty = 5,  .ord_type = OrderType::market,
        .price = std::nullopt, .time_in_force = TimeInForce::day,
    };

    REQUIRE(db.insert_order(301, order_a).has_value());
    REQUIRE(db.insert_order(302, order_b).has_value());
    REQUIRE(db.insert_order(303, order_c).has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= 3);

    // Verify all three are present.
    int found_count = 0;
    for (const auto& r : rows.value()) {
        if (r.order_id == 301) {
            CHECK(r.symbol == "AAPL");
            CHECK(r.side   == "BID");
            CHECK(r.price  == 15000);
            ++found_count;
        } else if (r.order_id == 302) {
            CHECK(r.symbol        == "GOOG");
            CHECK(r.side          == "ASK");
            CHECK(r.time_in_force == "GTC");
            ++found_count;
        } else if (r.order_id == 303) {
            CHECK(r.symbol   == "AAPL");
            CHECK(r.ord_type == "MARKET");
            CHECK(r.price    == 0);
            ++found_count;
        }
    }
    REQUIRE(found_count == 3);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

// ===========================================================================
//  COMPREHENSIVE TESTS – order lifecycle: insert then cancel
// ===========================================================================

TEST_CASE("order lifecycle: insert_order followed by insert_cancel produces two rows",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    // 1. Place order.
    NewOrderSingleContainer order{
        .sender_comp_id = "LIFECYCLE_CLIENT", .target_comp_id = "EXCHANGE",
        .cl_ord_id = "6001", .symbol = "AMZN", .side = Side::bid,
        .order_qty = 100, .ord_type = OrderType::limit,
        .price = 18500, .time_in_force = TimeInForce::day,
    };
    REQUIRE(db.insert_order(600, order).has_value());

    // 2. Cancel it.
    CancelOrderRequestContainer cancel{
        .sender_comp_id = "LIFECYCLE_CLIENT", .target_comp_id = "EXCHANGE",
        .order_id = "600", .orig_cl_ord_id = "6001",
        .cl_ord_id = "6002", .symbol = "AMZN",
        .side = Side::bid, .order_qty = 100,
    };
    REQUIRE(db.insert_cancel(cancel).has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    // QuestDB is append-only; both the NEW row and CANCELED row exist.
    REQUIRE(rows->size() >= 2);

    bool found_new = false, found_canceled = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 600 && r.order_status == "NEW") {
            CHECK(r.cl_order_id == 6001);
            found_new = true;
        }
        if (r.order_id == 600 && r.order_status == "CANCELED") {
            CHECK(r.cl_order_id == 6002);
            CHECK(r.filled_qty  == 0);
            found_canceled = true;
        }
    }
    CHECK(found_new);
    CHECK(found_canceled);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

// ===========================================================================
//  COMPREHENSIVE TESTS – insert_execution edge cases
// ===========================================================================

TEST_CASE("insert_execution fully filled order",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    ExecutionReportContainer exec{
        .sender_comp_id    = "FILL_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "800",
        .cl_order_id       = "8001",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E800",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_filled,
        .ord_status        = OrderStatus::status_filled,
        .ord_reject_reason = "",
        .symbol            = "NVDA",
        .side              = Side::ask,
        .price             = 45000,
        .time_in_force     = TimeInForce::day,
        .leaves_qty        = 0,   // nothing remaining
        .cum_qty           = 75,  // fully filled
        .avg_px            = 45000,
    };

    REQUIRE(db.insert_execution(exec).has_value());
    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 800 && r.cl_order_id == 8001) {
            CHECK(r.symbol        == "NVDA");
            CHECK(r.side          == "ASK");
            CHECK(r.order_qty     == 75);  // leaves(0) + cum(75)
            CHECK(r.filled_qty    == 75);  // cum_qty
            CHECK(r.price         == 45000);
            CHECK(r.order_status  == "FILLED");
            CHECK(r.time_in_force == "DAY");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

TEST_CASE("insert_execution with no time_in_force stores UNKNOWN",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    ExecutionReportContainer exec{
        .sender_comp_id    = "TIF_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "900",
        .cl_order_id       = "9001",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E900",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_new,
        .ord_status        = OrderStatus::status_new,
        .ord_reject_reason = "",
        .symbol            = "META",
        .side              = Side::bid,
        .price             = 50000,
        .time_in_force     = std::nullopt, // <-- no TIF
        .leaves_qty        = 40,
        .cum_qty           = 0,
        .avg_px            = 0,
    };

    REQUIRE(db.insert_execution(exec).has_value());
    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 900 && r.cl_order_id == 9001) {
            CHECK(r.time_in_force == "UNKNOWN");
            CHECK(r.order_status  == "NEW");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

TEST_CASE("insert_execution with no price stores 0",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    ExecutionReportContainer exec{
        .sender_comp_id    = "NOPRICE_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "950",
        .cl_order_id       = "9501",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E950",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_new,
        .ord_status        = OrderStatus::status_new,
        .ord_reject_reason = "",
        .symbol            = "TSLA",
        .side              = Side::ask,
        .price             = std::nullopt, // market-style, no price
        .time_in_force     = TimeInForce::day,
        .leaves_qty        = 15,
        .cum_qty           = 0,
        .avg_px            = 0,
    };

    REQUIRE(db.insert_execution(exec).has_value());
    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 950 && r.cl_order_id == 9501) {
            CHECK(r.price == 0); // nullopt -> 0
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

TEST_CASE("insert_execution progression: partial fill then full fill produces two rows",
          "[DatabaseClient][orders][comprehensive]") {
    DatabaseClient db;

    auto trunc = db.truncate_orders();
    REQUIRE(trunc.has_value());
    wait_for_questdb_ingestion();

    // First execution: partially filled (30 of 100 filled).
    ExecutionReportContainer partial{
        .sender_comp_id    = "PROG_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "700",
        .cl_order_id       = "7001",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E701",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_partially_filled,
        .ord_status        = OrderStatus::status_partially_filled,
        .ord_reject_reason = "",
        .symbol            = "GOOG",
        .side              = Side::bid,
        .price             = 17500,
        .time_in_force     = TimeInForce::gtc,
        .leaves_qty        = 70,
        .cum_qty           = 30,
        .avg_px            = 17500,
    };
    REQUIRE(db.insert_execution(partial).has_value());

    // Second execution: fully filled (remaining 70 filled).
    ExecutionReportContainer full{
        .sender_comp_id    = "PROG_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "700",
        .cl_order_id       = "7001",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E702",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_filled,
        .ord_status        = OrderStatus::status_filled,
        .ord_reject_reason = "",
        .symbol            = "GOOG",
        .side              = Side::bid,
        .price             = 17500,
        .time_in_force     = TimeInForce::gtc,
        .leaves_qty        = 0,
        .cum_qty           = 100,
        .avg_px            = 17500,
    };
    REQUIRE(db.insert_execution(full).has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    // Append-only: both execution rows exist.
    REQUIRE(rows->size() >= 2);

    bool found_partial = false, found_filled = false;
    for (const auto& r : rows.value()) {
        if (r.order_id != 700) continue;

        if (r.order_status == "PARTIALLY_FILLED") {
            CHECK(r.order_qty  == 100); // 70 + 30
            CHECK(r.filled_qty == 30);
            found_partial = true;
        } else if (r.order_status == "FILLED") {
            CHECK(r.order_qty  == 100); // 0 + 100
            CHECK(r.filled_qty == 100);
            found_filled = true;
        }
    }
    CHECK(found_partial);
    CHECK(found_filled);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

TEST_CASE("insert_execution succeeds and row is queryable", "[DatabaseClient][orders]") {
    DatabaseClient db;

    auto truncate_result = db.truncate_orders();
    REQUIRE(truncate_result.has_value());
    wait_for_questdb_ingestion();

    ExecutionReportContainer exec{
        .sender_comp_id    = "TEST_CLIENT",
        .target_comp_id    = "EXCHANGE",
        .order_id          = "7",
        .cl_order_id       = "2001",
        .orig_cl_ord_id    = std::nullopt,
        .exec_id           = "E001",
        .exec_trans_type   = ExecTransType::exec_trans_new,
        .exec_type         = ExecType::status_partially_filled,
        .ord_status        = OrderStatus::status_partially_filled,
        .ord_reject_reason = "",
        .symbol            = "MSFT",
        .side              = Side::bid,
        .price             = 30000, // $300.00 internal
        .time_in_force     = TimeInForce::gtc,
        .leaves_qty        = 20,
        .cum_qty           = 30,
        .avg_px            = 30000,
    };

    auto result = db.insert_execution(exec);
    REQUIRE(result.has_value());

    wait_for_questdb_ingestion();

    auto rows = db.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= 1);

    bool found = false;
    for (const auto& r : rows.value()) {
        if (r.order_id == 7 && r.cl_order_id == 2001) {
            CHECK(r.symbol         == "MSFT");
            CHECK(r.side           == "BID");
            CHECK(r.order_qty      == 50);   // leaves_qty + cum_qty = 20 + 30
            CHECK(r.filled_qty     == 30);   // cum_qty
            CHECK(r.price          == 30000);
            CHECK(r.time_in_force  == "GTC");
            CHECK(r.order_status   == "PARTIALLY_FILLED");
            found = true;
            break;
        }
    }
    REQUIRE(found);

    [[maybe_unused]] auto cleanup = db.truncate_orders();
}

// ===========================================================================
//  CONCURRENCY TESTS
// ===========================================================================

// ---------------------------------------------------------------------------
// Concurrency fixture for balance tests: seeds N users, each with a USD
// balance.  Each thread gets its own DatabaseClient (own connections).
// ---------------------------------------------------------------------------
static constexpr int CONC_NUM_USERS   = 8;
static constexpr int CONC_USER_BASE   = 90000; // user_ids 90000..90007

struct ConcurrencyBalanceFixture {
    pqxx::connection conn{CORE_DB_CONN_STR};

    ConcurrencyBalanceFixture() {
        cleanup_quietly();

        pqxx::work txn{conn};
        txn.exec(
            "INSERT INTO servers (server_id, server_name) VALUES ($1, $2)",
            pqxx::params{TEST_SERVER_ID, TEST_SERVER_NAME});
        for (int i = 0; i < CONC_NUM_USERS; ++i) {
            const int uid = CONC_USER_BASE + i;
            const auto uname = "conc_user_" + std::to_string(uid);
            txn.exec(
                "INSERT INTO users (user_id, username, password, created_ts, last_modified_ts) "
                "VALUES ($1, $2, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
                pqxx::params{uid, uname, TEST_PASSWORD});
            txn.exec(
                "INSERT INTO balances (user_id, symbol, balance, server_id, created_ts, modified_ts) "
                "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
                pqxx::params{uid, TEST_SYMBOL, 1000, TEST_SERVER_ID});
        }
        txn.commit();
    }

    ~ConcurrencyBalanceFixture() {
        cleanup_quietly();
    }

  private:
    void cleanup_quietly() {
        try {
            pqxx::work txn{conn};
            for (int i = 0; i < CONC_NUM_USERS; ++i) {
                const int uid = CONC_USER_BASE + i;
                txn.exec("DELETE FROM balances WHERE user_id = $1",
                         pqxx::params{uid});
                txn.exec("DELETE FROM users WHERE user_id = $1",
                         pqxx::params{uid});
            }
            txn.exec("DELETE FROM servers WHERE server_id = $1",
                     pqxx::params{TEST_SERVER_ID});
            txn.commit();
        } catch (...) {}
    }
};

// ---------------------------------------------------------------------------
// Concurrent reads: N threads each read their own user's balance
// simultaneously.  No thread should get an error or a wrong value.
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(ConcurrencyBalanceFixture,
                 "concurrent read_balance from multiple clients",
                 "[DatabaseClient][balance][concurrency]") {

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    for (int i = 0; i < CONC_NUM_USERS; ++i) {
        threads.emplace_back([i, &success_count, &failure_count]() {
            DatabaseClient db;
            const auto uid = CONC_USER_BASE + i;

            // Each thread reads its balance many times.
            for (int r = 0; r < 20; ++r) {
                auto result = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
                if (result.has_value() && result.value() == 1000) {
                    ++success_count;
                } else {
                    ++failure_count;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    CHECK(failure_count.load() == 0);
    REQUIRE(success_count.load() == CONC_NUM_USERS * 20);
}

// ---------------------------------------------------------------------------
// Concurrent writes: N threads each update their own (distinct) user's
// balance repeatedly.  After all threads finish, each user should have the
// final value written by its thread.
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(ConcurrencyBalanceFixture,
                 "concurrent update_balance to distinct users from multiple clients",
                 "[DatabaseClient][balance][concurrency]") {

    constexpr int NUM_UPDATES = 10;

    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    for (int i = 0; i < CONC_NUM_USERS; ++i) {
        threads.emplace_back([i, &error_count]() {
            DatabaseClient db;
            const auto uid = CONC_USER_BASE + i;

            for (int u = 1; u <= NUM_UPDATES; ++u) {
                auto res = db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, 1000 + u);
                if (!res.has_value()) ++error_count;
            }
        });
    }

    for (auto& t : threads) t.join();
    CHECK(error_count.load() == 0);

    // Verify final balances.
    DatabaseClient verifier;
    for (int i = 0; i < CONC_NUM_USERS; ++i) {
        const auto uid = CONC_USER_BASE + i;
        auto result = verifier.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
        REQUIRE(result.has_value());
        CHECK(result.value() == 1000 + NUM_UPDATES);
    }
}

// ---------------------------------------------------------------------------
// Concurrent writes to the SAME user: N threads all update_balance for the
// same user_id.  We don't know the interleaving, but every call should
// succeed (no crashes, no connection errors) and the final balance must
// equal one of the values that was written.
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(ConcurrencyBalanceFixture,
                 "concurrent update_balance to the same user from multiple clients",
                 "[DatabaseClient][balance][concurrency]") {

    constexpr int WRITES_PER_THREAD = 15;
    const auto uid = CONC_USER_BASE; // all target user 90000

    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    for (int i = 0; i < CONC_NUM_USERS; ++i) {
        threads.emplace_back([&uid, i, &error_count]() {
            DatabaseClient db;
            for (int w = 0; w < WRITES_PER_THREAD; ++w) {
                // Each thread writes a value that encodes thread id and iteration.
                int val = (i + 1) * 1000 + w;
                auto res = db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, val);
                if (!res.has_value()) ++error_count;
            }
        });
    }

    for (auto& t : threads) t.join();
    CHECK(error_count.load() == 0);

    // The final balance is deterministic only in that it must be one of the
    // values written.  Just verify it reads back without error.
    DatabaseClient verifier;
    auto result = verifier.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
    REQUIRE(result.has_value());
    // Value should be in range [1000, 8014] — (thread 1..8) * 1000 + (0..14).
    CHECK(result.value() >= 1000);
    CHECK(result.value() <= CONC_NUM_USERS * 1000 + (WRITES_PER_THREAD - 1));
}

// ---------------------------------------------------------------------------
// Concurrent mixed reads and writes: half the threads read, half write.
// All operations should succeed without crashing or returning connection
// errors.
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(ConcurrencyBalanceFixture,
                 "concurrent mixed read_balance and update_balance",
                 "[DatabaseClient][balance][concurrency]") {

    const auto uid = CONC_USER_BASE;
    constexpr int OPS_PER_THREAD = 20;

    std::atomic<int> read_errors{0};
    std::atomic<int> write_errors{0};
    std::vector<std::thread> threads;

    // Writer threads (even indices).
    for (int i = 0; i < CONC_NUM_USERS; i += 2) {
        threads.emplace_back([&uid, i, &write_errors]() {
            DatabaseClient db;
            for (int w = 0; w < OPS_PER_THREAD; ++w) {
                auto res = db.update_balance(uid, TEST_SERVER_ID, TEST_SYMBOL, (i + 1) * 100 + w);
                if (!res.has_value()) ++write_errors;
            }
        });
    }

    // Reader threads (odd indices).
    for (int i = 1; i < CONC_NUM_USERS; i += 2) {
        threads.emplace_back([&uid, &read_errors]() {
            DatabaseClient db;
            for (int r = 0; r < OPS_PER_THREAD; ++r) {
                auto res = db.read_balance(uid, TEST_SERVER_ID, TEST_SYMBOL);
                if (!res.has_value()) ++read_errors;
            }
        });
    }

    for (auto& t : threads) t.join();

    CHECK(read_errors.load()  == 0);
    CHECK(write_errors.load() == 0);
}

// ===========================================================================
//  CONCURRENCY TESTS – QuestDB orders (insert_order / insert_cancel /
//  insert_execution via ILP, query_orders via PG wire)
// ===========================================================================

TEST_CASE("concurrent insert_order from multiple clients",
          "[DatabaseClient][orders][concurrency]") {
    {
        DatabaseClient setup;
        auto trunc = setup.truncate_orders();
        REQUIRE(trunc.has_value());
    }
    wait_for_questdb_ingestion();

    constexpr int NUM_THREADS      = 4;
    constexpr int ORDERS_PER_THREAD = 5;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t, &error_count]() {
            DatabaseClient db;
            for (int o = 0; o < ORDERS_PER_THREAD; ++o) {
                int oid = t * 1000 + o;
                NewOrderSingleContainer order{
                    .sender_comp_id = "CONC_CLIENT_" + std::to_string(t),
                    .target_comp_id = "EXCHANGE",
                    .cl_ord_id      = std::to_string(oid + 10000),
                    .symbol         = "AAPL",
                    .side           = (o % 2 == 0) ? Side::bid : Side::ask,
                    .order_qty      = 10 + o,
                    .ord_type       = OrderType::limit,
                    .price          = 15000 + o * 100,
                    .time_in_force  = TimeInForce::day,
                };
                auto res = db.insert_order(oid, order);
                if (!res.has_value()) ++error_count;
            }
        });
    }

    for (auto& th : threads) th.join();
    CHECK(error_count.load() == 0);

    wait_for_questdb_ingestion();

    DatabaseClient verifier;
    auto rows = verifier.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= static_cast<size_t>(NUM_THREADS * ORDERS_PER_THREAD));

    // Build a set of order_ids we expect.
    std::set<int> expected_ids;
    for (int t = 0; t < NUM_THREADS; ++t)
        for (int o = 0; o < ORDERS_PER_THREAD; ++o)
            expected_ids.insert(t * 1000 + o);

    std::set<int> found_ids;
    for (const auto& r : rows.value())
        if (expected_ids.count(r.order_id))
            found_ids.insert(r.order_id);

    REQUIRE(found_ids == expected_ids);

    [[maybe_unused]] auto cleanup = verifier.truncate_orders();
}

TEST_CASE("concurrent insert_cancel from multiple clients",
          "[DatabaseClient][orders][concurrency]") {
    {
        DatabaseClient setup;
        auto trunc = setup.truncate_orders();
        REQUIRE(trunc.has_value());
    }
    wait_for_questdb_ingestion();

    constexpr int NUM_THREADS       = 4;
    constexpr int CANCELS_PER_THREAD = 5;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t, &error_count]() {
            DatabaseClient db;
            for (int c = 0; c < CANCELS_PER_THREAD; ++c) {
                int oid = t * 1000 + c;
                CancelOrderRequestContainer cancel{
                    .sender_comp_id = "CONC_CANCEL_" + std::to_string(t),
                    .target_comp_id = "EXCHANGE",
                    .order_id       = std::to_string(oid),
                    .orig_cl_ord_id = std::to_string(oid + 10000),
                    .cl_ord_id      = std::to_string(oid + 20000),
                    .symbol         = "GOOG",
                    .side           = Side::bid,
                    .order_qty      = 15,
                };
                auto res = db.insert_cancel(cancel);
                if (!res.has_value()) ++error_count;
            }
        });
    }

    for (auto& th : threads) th.join();
    CHECK(error_count.load() == 0);

    wait_for_questdb_ingestion();

    DatabaseClient verifier;
    auto rows = verifier.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= static_cast<size_t>(NUM_THREADS * CANCELS_PER_THREAD));

    int canceled_count = 0;
    for (const auto& r : rows.value())
        if (r.order_status == "CANCELED" && r.symbol == "GOOG")
            ++canceled_count;

    REQUIRE(canceled_count >= NUM_THREADS * CANCELS_PER_THREAD);

    [[maybe_unused]] auto cleanup = verifier.truncate_orders();
}

TEST_CASE("concurrent insert_execution from multiple clients",
          "[DatabaseClient][orders][concurrency]") {
    {
        DatabaseClient setup;
        auto trunc = setup.truncate_orders();
        REQUIRE(trunc.has_value());
    }
    wait_for_questdb_ingestion();

    constexpr int NUM_THREADS     = 4;
    constexpr int EXECS_PER_THREAD = 5;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t, &error_count]() {
            DatabaseClient db;
            for (int e = 0; e < EXECS_PER_THREAD; ++e) {
                int oid = t * 1000 + e;
                ExecutionReportContainer exec{
                    .sender_comp_id    = "CONC_EXEC_" + std::to_string(t),
                    .target_comp_id    = "EXCHANGE",
                    .order_id          = std::to_string(oid),
                    .cl_order_id       = std::to_string(oid + 10000),
                    .orig_cl_ord_id    = std::nullopt,
                    .exec_id           = "CE_" + std::to_string(oid),
                    .exec_trans_type   = ExecTransType::exec_trans_new,
                    .exec_type         = ExecType::status_filled,
                    .ord_status        = OrderStatus::status_filled,
                    .ord_reject_reason = "",
                    .symbol            = "NVDA",
                    .side              = Side::ask,
                    .price             = 40000 + e * 100,
                    .time_in_force     = TimeInForce::day,
                    .leaves_qty        = 0,
                    .cum_qty           = 50 + e,
                    .avg_px            = 40000 + e * 100,
                };
                auto res = db.insert_execution(exec);
                if (!res.has_value()) ++error_count;
            }
        });
    }

    for (auto& th : threads) th.join();
    CHECK(error_count.load() == 0);

    wait_for_questdb_ingestion();

    DatabaseClient verifier;
    auto rows = verifier.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= static_cast<size_t>(NUM_THREADS * EXECS_PER_THREAD));

    int filled_count = 0;
    for (const auto& r : rows.value())
        if (r.order_status == "FILLED" && r.symbol == "NVDA")
            ++filled_count;

    REQUIRE(filled_count >= NUM_THREADS * EXECS_PER_THREAD);

    [[maybe_unused]] auto cleanup = verifier.truncate_orders();
}

// ---------------------------------------------------------------------------
// Mixed concurrent workload: threads simultaneously insert orders, cancels,
// and executions.  All calls must succeed and the total row count must match.
// ---------------------------------------------------------------------------
TEST_CASE("concurrent mixed insert_order, insert_cancel, insert_execution",
          "[DatabaseClient][orders][concurrency]") {
    {
        DatabaseClient setup;
        auto trunc = setup.truncate_orders();
        REQUIRE(trunc.has_value());
    }
    wait_for_questdb_ingestion();

    constexpr int OPS_PER_TYPE = 4; // per thread-type

    std::atomic<int> order_errors{0};
    std::atomic<int> cancel_errors{0};
    std::atomic<int> exec_errors{0};
    std::vector<std::thread> threads;

    // 2 threads inserting orders.
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([t, &order_errors]() {
            DatabaseClient db;
            for (int o = 0; o < OPS_PER_TYPE; ++o) {
                int oid = 10000 + t * 100 + o;
                NewOrderSingleContainer order{
                    .sender_comp_id = "MIX_ORD_" + std::to_string(t),
                    .target_comp_id = "EXCHANGE",
                    .cl_ord_id      = std::to_string(oid + 50000),
                    .symbol         = "AAPL",
                    .side           = Side::bid,
                    .order_qty      = 10,
                    .ord_type       = OrderType::limit,
                    .price          = 15000,
                    .time_in_force  = TimeInForce::day,
                };
                if (!db.insert_order(oid, order).has_value()) ++order_errors;
            }
        });
    }

    // 2 threads inserting cancels.
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([t, &cancel_errors]() {
            DatabaseClient db;
            for (int c = 0; c < OPS_PER_TYPE; ++c) {
                int oid = 20000 + t * 100 + c;
                CancelOrderRequestContainer cancel{
                    .sender_comp_id = "MIX_CXL_" + std::to_string(t),
                    .target_comp_id = "EXCHANGE",
                    .order_id       = std::to_string(oid),
                    .orig_cl_ord_id = std::to_string(oid + 50000),
                    .cl_ord_id      = std::to_string(oid + 60000),
                    .symbol         = "AAPL",
                    .side           = Side::ask,
                    .order_qty      = 10,
                };
                if (!db.insert_cancel(cancel).has_value()) ++cancel_errors;
            }
        });
    }

    // 2 threads inserting executions.
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([t, &exec_errors]() {
            DatabaseClient db;
            for (int e = 0; e < OPS_PER_TYPE; ++e) {
                int oid = 30000 + t * 100 + e;
                ExecutionReportContainer exec{
                    .sender_comp_id    = "MIX_EXEC_" + std::to_string(t),
                    .target_comp_id    = "EXCHANGE",
                    .order_id          = std::to_string(oid),
                    .cl_order_id       = std::to_string(oid + 50000),
                    .orig_cl_ord_id    = std::nullopt,
                    .exec_id           = "ME_" + std::to_string(oid),
                    .exec_trans_type   = ExecTransType::exec_trans_new,
                    .exec_type         = ExecType::status_partially_filled,
                    .ord_status        = OrderStatus::status_partially_filled,
                    .ord_reject_reason = "",
                    .symbol            = "AAPL",
                    .side              = Side::bid,
                    .price             = 15000,
                    .time_in_force     = TimeInForce::day,
                    .leaves_qty        = 5,
                    .cum_qty           = 5,
                    .avg_px            = 15000,
                };
                if (!db.insert_execution(exec).has_value()) ++exec_errors;
            }
        });
    }

    for (auto& th : threads) th.join();

    CHECK(order_errors.load()  == 0);
    CHECK(cancel_errors.load() == 0);
    CHECK(exec_errors.load()   == 0);

    wait_for_questdb_ingestion();

    // Total expected rows: 2 * OPS_PER_TYPE * 3 types = 24.
    constexpr int EXPECTED_TOTAL = 2 * OPS_PER_TYPE * 3;

    DatabaseClient verifier;
    auto rows = verifier.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() >= static_cast<size_t>(EXPECTED_TOTAL));

    [[maybe_unused]] auto cleanup = verifier.truncate_orders();
}

// ---------------------------------------------------------------------------
// Concurrent query_orders (reads) while another thread is inserting.
// All reads must succeed; no crashes or connection errors.
// ---------------------------------------------------------------------------
TEST_CASE("concurrent query_orders reads while inserting",
          "[DatabaseClient][orders][concurrency]") {
    {
        DatabaseClient setup;
        auto trunc = setup.truncate_orders();
        REQUIRE(trunc.has_value());
    }
    wait_for_questdb_ingestion();

    constexpr int NUM_INSERTS  = 10;
    constexpr int NUM_READERS  = 3;
    constexpr int READS_PER    = 10;

    std::atomic<bool> writer_done{false};
    std::atomic<int> write_errors{0};
    std::atomic<int> read_errors{0};

    // Writer thread: inserts orders one by one.
    std::thread writer([&]() {
        DatabaseClient db;
        for (int o = 0; o < NUM_INSERTS; ++o) {
            NewOrderSingleContainer order{
                .sender_comp_id = "WRITER",
                .target_comp_id = "EXCHANGE",
                .cl_ord_id      = std::to_string(40000 + o),
                .symbol         = "TSLA",
                .side           = Side::bid,
                .order_qty      = 1,
                .ord_type       = OrderType::limit,
                .price          = 20000,
                .time_in_force  = TimeInForce::day,
            };
            if (!db.insert_order(40000 + o, order).has_value()) ++write_errors;
            // Small stagger so reads interleave with writes.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        writer_done.store(true);
    });

    // Reader threads: repeatedly query_orders.
    std::vector<std::thread> readers;
    for (int r = 0; r < NUM_READERS; ++r) {
        readers.emplace_back([&]() {
            DatabaseClient db;
            int ops = 0;
            while (!writer_done.load() || ops < READS_PER) {
                auto rows = db.query_orders();
                if (!rows.has_value()) ++read_errors;
                ++ops;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        });
    }

    writer.join();
    for (auto& t : readers) t.join();

    CHECK(write_errors.load() == 0);
    CHECK(read_errors.load()  == 0);

    [[maybe_unused]] DatabaseClient cleanup_db;
    [[maybe_unused]] auto cleanup = cleanup_db.truncate_orders();
}
