#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <database/database_client.h>
#include <core/containers.h>
#include <core/orders.h>
#include <pqxx/pqxx>
#include <thread>
#include <chrono>

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
static constexpr int         TEST_USER_ID       = 99999;
static constexpr const char* TEST_USERNAME       = "test_db_user_99999";
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
            "INSERT INTO balances (user_id, symbol, balance, created_ts, modified_ts) "
            "VALUES ($1, $2, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            pqxx::params{TEST_USER_ID, TEST_SYMBOL, TEST_INITIAL_BALANCE});
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
    auto result = db.read_balance(std::to_string(TEST_USER_ID), TEST_SYMBOL);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == TEST_INITIAL_BALANCE);
}

TEST_CASE_METHOD(BalanceFixture,
                 "read_balance returns error for non-existing user",
                 "[DatabaseClient][balance]") {
    auto result = db.read_balance("non_existing_user_xyz_000", TEST_SYMBOL);
    // No row should match, so the query_value call throws -> std::unexpected.
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(BalanceFixture,
                 "update_balance changes the balance and read_balance reflects it",
                 "[DatabaseClient][balance]") {
    const int new_balance = TEST_INITIAL_BALANCE + 100;

    auto update_result = db.update_balance(
        std::to_string(TEST_USER_ID), TEST_SYMBOL, new_balance);
    REQUIRE(update_result.has_value());

    auto after_update = db.read_balance(
        std::to_string(TEST_USER_ID), TEST_SYMBOL);
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
//  insert_execution  (QuestDB – orders table via ILP)
// ===========================================================================

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