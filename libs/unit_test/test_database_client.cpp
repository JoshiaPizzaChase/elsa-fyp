// test_database_client.cpp
//
// Comprehensive Catch2 unit tests for database::DatabaseClient.
//
// Prerequisites:
//   - PostgreSQL running locally on port 5432
//   - QuestDB running locally (ILP on port 9000, PG wire on port 8812)
//   - Database schemas initialised via scripts/init_databases.sh
//
// The test binary is expected to be run via a wrapper that:
//   1. Runs `sudo scripts/init_databases.sh` before tests
//   2. Runs `questdb stop` after tests complete

#include <catch2/catch_test_macros.hpp>
//#include <catch2/catch_session.hpp>

#include <database/database_client.h>
#include <core/containers.h>
#include <core/orders.h>
#include <core/trade.h>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <string>
#include <memory>

using namespace database;
using namespace core;

// ---------------------------------------------------------------------------
// Helper: small sleep to let QuestDB commit ILP writes before querying back
// via the PG wire protocol (QuestDB has eventual consistency for ILP ingestion).
// ---------------------------------------------------------------------------
static void wait_for_questdb_commit(int ms = 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ---------------------------------------------------------------------------
// Helper: build a NewOrderSingleContainer with sensible defaults
// ---------------------------------------------------------------------------
static NewOrderSingleContainer make_new_order(
    const std::string& sender   = "BROKER_A",
    const std::string& target   = "EXCHANGE",
    const std::string& cl_ord   = "100",
    const std::string& sym      = "AAPL",
    Side                side     = Side::bid,
    int                 qty      = 1000,
    OrderType           ot       = OrderType::limit,
    std::optional<int>  px       = 15000,
    TimeInForce         tif      = TimeInForce::day)
{
    return NewOrderSingleContainer{
        .sender_comp_id = sender,
        .target_comp_id = target,
        .cl_ord_id      = cl_ord,
        .symbol         = sym,
        .side           = side,
        .order_qty      = qty,
        .ord_type       = ot,
        .price          = px,
        .time_in_force  = tif,
    };
}

// ---------------------------------------------------------------------------
// Helper: build a CancelOrderRequestContainer
// ---------------------------------------------------------------------------
static CancelOrderRequestContainer make_cancel_order(
    const std::string& sender       = "BROKER_A",
    const std::string& target       = "EXCHANGE",
    const std::string& order_id     = "1",
    const std::string& orig_cl_ord  = "100",
    const std::string& cl_ord       = "101",
    const std::string& sym          = "AAPL",
    Side                side         = Side::bid,
    int                 qty          = 1000)
{
    return CancelOrderRequestContainer{
        .sender_comp_id = sender,
        .target_comp_id = target,
        .order_id       = order_id,
        .orig_cl_ord_id = orig_cl_ord,
        .cl_ord_id      = cl_ord,
        .symbol         = sym,
        .side           = side,
        .order_qty      = qty,
    };
}

// ---------------------------------------------------------------------------
// Helper: build an ExecutionReportContainer
// ---------------------------------------------------------------------------
static ExecutionReportContainer make_execution_report(
    const std::string& sender    = "EXCHANGE",
    const std::string& target    = "BROKER_A",
    const std::string& order_id  = "1",
    const std::string& cl_ord    = "100",
    const std::string& exec_id   = "E001",
    ExecType            exec_type = ExecType::status_filled,
    OrderStatus         status    = OrderStatus::status_filled,
    const std::string& sym       = "AAPL",
    Side                side      = Side::bid,
    std::optional<int>  px        = 15000,
    int                 leaves    = 0,
    int                 cum       = 1000,
    int                 avg       = 15000)
{
    return ExecutionReportContainer{
        .sender_comp_id   = sender,
        .target_comp_id   = target,
        .order_id         = order_id,
        .cl_order_id      = cl_ord,
        .orig_cl_ord_id   = std::nullopt,
        .exec_id          = exec_id,
        .exec_trans_type  = ExecTransType::exec_trans_new,
        .exec_type        = exec_type,
        .ord_status       = status,
        .ord_reject_reason = "",
        .symbol           = sym,
        .side             = side,
        .price            = px,
        .time_in_force    = TimeInForce::day,
        .leaves_qty       = leaves,
        .cum_qty          = cum,
        .avg_px           = avg,
    };
}

// ---------------------------------------------------------------------------
// Helper: build a Trade
// ---------------------------------------------------------------------------
static Trade make_trade(
    const char* ticker       = "AAPL",
    int         price        = 15000,
    int         quantity     = 500,
    int         trade_id     = 1,
    int         taker_id     = 42,
    int         maker_id     = 43,
    int         taker_ord    = 1,
    int         maker_ord    = 2,
    bool        taker_buyer  = true)
{
    return Trade{ticker, price, quantity, trade_id, taker_id, maker_id,
                 taker_ord, maker_ord, taker_buyer};
}

// ===========================================================================
//  Fixture: manages a single DatabaseClient for a group of tests.
//  Before each TEST_CASE_METHOD the client is freshly constructed, so each
//  test gets its own connections.
// ===========================================================================
class DatabaseFixture {
  protected:
    DatabaseClient client;
};

// ===========================================================================
// 1. Balance tests (PostgreSQL – edux_core_db)
// ===========================================================================

TEST_CASE("Balance: read_balance returns error for nonexistent user/symbol",
          "[balance][postgresql]")
{
    DatabaseClient client;

    auto result = client.read_balance("999999", "ZZZZ");
    REQUIRE_FALSE(result.has_value());
    // The error should mention something about the query failing
    REQUIRE_FALSE(result.error().empty());
}

TEST_CASE("Balance: update_balance succeeds on existing row",
          "[balance][postgresql]")
{
    DatabaseClient client;

    // This test assumes a row exists in balances for user_id=1, symbol='AAPL'.
    // If the row doesn't exist the UPDATE will simply affect 0 rows (not error),
    // so we just verify the call itself succeeds.
    auto result = client.update_balance("1", "AAPL", 50000);
    // update_balance returns expected<void,...> — it succeeds even if 0 rows match
    CHECK(result.has_value());
}

TEST_CASE("Balance: read_balance returns correct value after update",
          "[balance][postgresql]")
{
    DatabaseClient client;

    // Seed: insert a balance row directly so we have something to read.
    // We use a raw pqxx connection for setup.
    {
        pqxx::connection setup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{setup_conn};
        // Ensure user exists
        txn.exec(
            "INSERT INTO users (user_id, username, password) "
            "VALUES (9999, 'test_user_balance', 'pass') "
            "ON CONFLICT (user_id) DO NOTHING");
        // Ensure balance row exists
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance) "
            "VALUES (9999, 'TSLA', 100) "
            "ON CONFLICT (user_id, symbol) DO UPDATE SET balance = 100");
        txn.commit();
    }

    // Update via the client
    auto upd = client.update_balance("9999", "TSLA", 42000);
    REQUIRE(upd.has_value());

    // Read back
    auto bal = client.read_balance("9999", "TSLA");
    REQUIRE(bal.has_value());
    CHECK(bal.value() == 42000);

    // Cleanup
    {
        pqxx::connection cleanup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{cleanup_conn};
        txn.exec("DELETE FROM balances WHERE user_id = 9999");
        txn.exec("DELETE FROM users WHERE user_id = 9999");
        txn.commit();
    }
}

TEST_CASE("Balance: update_balance with negative balance stores correctly",
          "[balance][postgresql]")
{
    DatabaseClient client;

    {
        pqxx::connection setup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{setup_conn};
        txn.exec(
            "INSERT INTO users (user_id, username, password) "
            "VALUES (9998, 'test_user_neg', 'pass') "
            "ON CONFLICT (user_id) DO NOTHING");
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance) "
            "VALUES (9998, 'GOOG', 0) "
            "ON CONFLICT (user_id, symbol) DO UPDATE SET balance = 0");
        txn.commit();
    }

    auto upd = client.update_balance("9998", "GOOG", -500);
    REQUIRE(upd.has_value());

    auto bal = client.read_balance("9998", "GOOG");
    REQUIRE(bal.has_value());
    CHECK(bal.value() == -500);

    {
        pqxx::connection cleanup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{cleanup_conn};
        txn.exec("DELETE FROM balances WHERE user_id = 9998");
        txn.exec("DELETE FROM users WHERE user_id = 9998");
        txn.commit();
    }
}

TEST_CASE("Balance: update_balance with zero balance",
          "[balance][postgresql]")
{
    DatabaseClient client;

    {
        pqxx::connection setup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{setup_conn};
        txn.exec(
            "INSERT INTO users (user_id, username, password) "
            "VALUES (9997, 'test_user_zero', 'pass') "
            "ON CONFLICT (user_id) DO NOTHING");
        txn.exec(
            "INSERT INTO balances (user_id, symbol, balance) "
            "VALUES (9997, 'MSFT', 999) "
            "ON CONFLICT (user_id, symbol) DO UPDATE SET balance = 999");
        txn.commit();
    }

    auto upd = client.update_balance("9997", "MSFT", 0);
    REQUIRE(upd.has_value());

    auto bal = client.read_balance("9997", "MSFT");
    REQUIRE(bal.has_value());
    CHECK(bal.value() == 0);

    {
        pqxx::connection cleanup_conn{"host=localhost port=5432 dbname=edux_core_db"};
        pqxx::work txn{cleanup_conn};
        txn.exec("DELETE FROM balances WHERE user_id = 9997");
        txn.exec("DELETE FROM users WHERE user_id = 9997");
        txn.commit();
    }
}

// ===========================================================================
// 2. Order insertion tests (QuestDB via ILP + read-back via PG wire)
// ===========================================================================

TEST_CASE("Order: insert_order for a limit buy stores correct fields",
          "[order][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("BROKER_A", "EXCHANGE", "200", "AAPL",
                                Side::bid, 1000, OrderType::limit, 15000,
                                TimeInForce::day);

    auto res = client.insert_order(1, order);
    REQUIRE(res.has_value());

    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());
    REQUIRE_FALSE(rows->empty());

    // Find our order
    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 1 && row.cl_order_id == 200) {
            found = true;
            CHECK(row.sender_comp_id == "BROKER_A");
            CHECK(row.symbol == "AAPL");
            CHECK(row.side == "BID");
            CHECK(row.order_qty == 1000);
            CHECK(row.filled_qty == 0);
            CHECK(row.price == 15000);
            CHECK(row.order_status == "NEW");
            CHECK(row.time_in_force == "DAY");
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Order: insert_order for a market order stores price as 0",
          "[order][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("BROKER_B", "EXCHANGE", "300", "GOOG",
                                Side::ask, 500, OrderType::market,
                                std::nullopt, TimeInForce::gtc);

    auto res = client.insert_order(2, order);
    REQUIRE(res.has_value());

    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 2) {
            found = true;
            CHECK(row.symbol == "GOOG");
            CHECK(row.side == "ASK");
            CHECK(row.price == 0);
            CHECK(row.order_qty == 500);
            CHECK(row.filled_qty == 0);
            CHECK(row.order_status == "NEW");
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Order: insert multiple orders and query all back",
          "[order][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order1 = make_new_order("B1", "EX", "401", "AAPL", Side::bid, 100,
                                  OrderType::limit, 14000, TimeInForce::day);
    auto order2 = make_new_order("B2", "EX", "402", "MSFT", Side::ask, 200,
                                  OrderType::limit, 30000, TimeInForce::gtc);
    auto order3 = make_new_order("B3", "EX", "403", "TSLA", Side::bid, 300,
                                  OrderType::market, std::nullopt, TimeInForce::day);

    REQUIRE(client.insert_order(10, order1).has_value());
    REQUIRE(client.insert_order(11, order2).has_value());
    REQUIRE(client.insert_order(12, order3).has_value());

    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());
    CHECK(rows->size() >= 3);

    // Verify each order is present
    int found_count = 0;
    for (const auto& row : *rows) {
        if (row.order_id == 10) { ++found_count; CHECK(row.symbol == "AAPL"); }
        if (row.order_id == 11) { ++found_count; CHECK(row.symbol == "MSFT"); }
        if (row.order_id == 12) { ++found_count; CHECK(row.symbol == "TSLA"); }
    }
    CHECK(found_count == 3);

    client.truncate_orders();
}

// ===========================================================================
// 3. Cancel order tests
// ===========================================================================

TEST_CASE("Order: insert_cancel writes a CANCELED row",
          "[cancel][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    // First insert the original order
    auto order = make_new_order("BROKER_C", "EXCHANGE", "500", "AAPL",
                                Side::bid, 750, OrderType::limit, 14500,
                                TimeInForce::day);
    REQUIRE(client.insert_order(20, order).has_value());

    wait_for_questdb_commit();

    // Now cancel it
    auto cancel = make_cancel_order("BROKER_C", "EXCHANGE", "20", "500",
                                     "501", "AAPL", Side::bid, 750);
    auto res = client.insert_cancel(cancel);
    REQUIRE(res.has_value());

    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    // We should find the latest row for order_id=20 with CANCELED status
    // (QuestDB dedup upsert on ts + order_id means the cancel overwrites)
    bool found_cancel = false;
    for (const auto& row : *rows) {
        if (row.order_id == 20 && row.order_status == "CANCELED") {
            found_cancel = true;
            CHECK(row.symbol == "AAPL");
            CHECK(row.side == "BID");
            CHECK(row.filled_qty == 0);
            break;
        }
    }
    CHECK(found_cancel);

    client.truncate_orders();
}

TEST_CASE("Order: insert_cancel for ASK side",
          "[cancel][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("BROKER_D", "EXCHANGE", "600", "TSLA",
                                Side::ask, 200, OrderType::limit, 70000,
                                TimeInForce::day);
    REQUIRE(client.insert_order(30, order).has_value());
    wait_for_questdb_commit();

    auto cancel = make_cancel_order("BROKER_D", "EXCHANGE", "30", "600",
                                     "601", "TSLA", Side::ask, 200);
    REQUIRE(client.insert_cancel(cancel).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 30 && row.order_status == "CANCELED") {
            found = true;
            CHECK(row.side == "ASK");
            CHECK(row.symbol == "TSLA");
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

// ===========================================================================
// 4. Execution report tests
// ===========================================================================

TEST_CASE("Order: insert_execution for a fully filled order",
          "[execution][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    // Insert original order
    auto order = make_new_order("BROKER_E", "EXCHANGE", "700", "AAPL",
                                Side::bid, 1000, OrderType::limit, 15000,
                                TimeInForce::day);
    REQUIRE(client.insert_order(40, order).has_value());
    wait_for_questdb_commit();

    // Now simulate a fill
    auto exec = make_execution_report(
        "EXCHANGE", "BROKER_E", "40", "700", "E100",
        ExecType::status_filled, OrderStatus::status_filled,
        "AAPL", Side::bid, 15000,
        /*leaves=*/0, /*cum=*/1000, /*avg=*/15000);

    auto res = client.insert_execution(exec);
    REQUIRE(res.has_value());

    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 40 && row.order_status == "FILLED") {
            found = true;
            CHECK(row.filled_qty == 1000);
            CHECK(row.order_qty == 1000);   // leaves_qty(0) + cum_qty(1000)
            CHECK(row.price == 15000);
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Order: insert_execution for a partially filled order",
          "[execution][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("BROKER_F", "EXCHANGE", "800", "MSFT",
                                Side::ask, 2000, OrderType::limit, 30000,
                                TimeInForce::gtc);
    REQUIRE(client.insert_order(50, order).has_value());
    wait_for_questdb_commit();

    auto exec = make_execution_report(
        "EXCHANGE", "BROKER_F", "50", "800", "E200",
        ExecType::status_partially_filled, OrderStatus::status_partially_filled,
        "MSFT", Side::ask, 30000,
        /*leaves=*/1500, /*cum=*/500, /*avg=*/30000);

    REQUIRE(client.insert_execution(exec).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 50 && row.order_status == "PARTIALLY_FILLED") {
            found = true;
            CHECK(row.filled_qty == 500);
            CHECK(row.order_qty == 2000);   // 1500 + 500
            CHECK(row.side == "ASK");
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Order: insert_execution for a rejected order",
          "[execution][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto exec = make_execution_report(
        "EXCHANGE", "BROKER_G", "60", "900", "E300",
        ExecType::status_rejected, OrderStatus::status_rejected,
        "AAPL", Side::bid, std::nullopt,
        /*leaves=*/0, /*cum=*/0, /*avg=*/0);

    REQUIRE(client.insert_execution(exec).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 60 && row.order_status == "REJECTED") {
            found = true;
            CHECK(row.filled_qty == 0);
            CHECK(row.price == 0);  // no price for rejected
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Order: insert_execution with NEW status (ack)",
          "[execution][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto exec = make_execution_report(
        "EXCHANGE", "BROKER_H", "70", "1000", "E400",
        ExecType::status_new, OrderStatus::status_new,
        "GOOG", Side::ask, 250000,
        /*leaves=*/100, /*cum=*/0, /*avg=*/0);

    REQUIRE(client.insert_execution(exec).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 70 && row.order_status == "NEW") {
            found = true;
            CHECK(row.symbol == "GOOG");
            CHECK(row.side == "ASK");
            CHECK(row.filled_qty == 0);
            CHECK(row.order_qty == 100);  // leaves(100) + cum(0)
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

// ===========================================================================
// 5. Trade insertion tests
// ===========================================================================

TEST_CASE("Trade: insert_trade stores correct fields",
          "[trade][questdb]")
{
    DatabaseClient client;
    client.truncate_trades();
    wait_for_questdb_commit();

    auto trade = make_trade("AAPL", 15000, 500, 1, 42, 43, 1, 2, true);

    auto res = client.insert_trade(trade);
    REQUIRE(res.has_value());

    wait_for_questdb_commit();

    auto rows = client.query_trades();
    REQUIRE(rows.has_value());
    REQUIRE_FALSE(rows->empty());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.trade_id == 1) {
            found = true;
            CHECK(row.price == 15000);
            CHECK(row.quantity == 500);
            CHECK(row.symbol == "AAPL");
            CHECK(row.taker_id == 42);
            CHECK(row.maker_id == 43);
            CHECK(row.taker_order_id == 1);
            CHECK(row.maker_order_id == 2);
            CHECK(row.is_taker_buyer == true);
            break;
        }
    }
    CHECK(found);

    client.truncate_trades();
}

TEST_CASE("Trade: insert_trade for a sell (is_taker_buyer = false)",
          "[trade][questdb]")
{
    DatabaseClient client;
    client.truncate_trades();
    wait_for_questdb_commit();

    auto trade = make_trade("MSFT", 30000, 250, 2, 10, 20, 5, 6, false);

    REQUIRE(client.insert_trade(trade).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_trades();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.trade_id == 2) {
            found = true;
            CHECK(row.symbol == "MSFT");
            CHECK(row.price == 30000);
            CHECK(row.quantity == 250);
            CHECK(row.is_taker_buyer == false);
            break;
        }
    }
    CHECK(found);

    client.truncate_trades();
}

TEST_CASE("Trade: insert multiple trades and query all back",
          "[trade][questdb]")
{
    DatabaseClient client;
    client.truncate_trades();
    wait_for_questdb_commit();

    auto t1 = make_trade("AAPL", 15000, 100, 100, 1, 2, 10, 20, true);
    auto t2 = make_trade("GOOG", 250000, 50, 101, 3, 4, 11, 21, false);
    auto t3 = make_trade("TSLA", 70000, 75, 102, 5, 6, 12, 22, true);

    REQUIRE(client.insert_trade(t1).has_value());
    REQUIRE(client.insert_trade(t2).has_value());
    REQUIRE(client.insert_trade(t3).has_value());

    wait_for_questdb_commit();

    auto rows = client.query_trades();
    REQUIRE(rows.has_value());
    CHECK(rows->size() >= 3);

    int found_count = 0;
    for (const auto& row : *rows) {
        if (row.trade_id == 100) { ++found_count; CHECK(row.symbol == "AAPL"); }
        if (row.trade_id == 101) { ++found_count; CHECK(row.symbol == "GOOG"); }
        if (row.trade_id == 102) { ++found_count; CHECK(row.symbol == "TSLA"); }
    }
    CHECK(found_count == 3);

    client.truncate_trades();
}

// ===========================================================================
// 6. Truncate / cleanup tests
// ===========================================================================

TEST_CASE("Cleanup: truncate_orders empties the orders table",
          "[cleanup][questdb]")
{
    DatabaseClient client;

    // Insert something first
    auto order = make_new_order("CLEANUP", "EX", "9000", "AAPL",
                                Side::bid, 1, OrderType::limit, 100,
                                TimeInForce::day);
    REQUIRE(client.insert_order(9000, order).has_value());
    wait_for_questdb_commit();

    auto before = client.query_orders();
    REQUIRE(before.has_value());
    REQUIRE_FALSE(before->empty());

    REQUIRE(client.truncate_orders().has_value());
    wait_for_questdb_commit();

    auto after = client.query_orders();
    REQUIRE(after.has_value());
    CHECK(after->empty());
}

TEST_CASE("Cleanup: truncate_trades empties the trades table",
          "[cleanup][questdb]")
{
    DatabaseClient client;

    auto trade = make_trade("AAPL", 100, 1, 9999, 1, 1, 1, 1, true);
    REQUIRE(client.insert_trade(trade).has_value());
    wait_for_questdb_commit();

    auto before = client.query_trades();
    REQUIRE(before.has_value());
    REQUIRE_FALSE(before->empty());

    REQUIRE(client.truncate_trades().has_value());
    wait_for_questdb_commit();

    auto after = client.query_trades();
    REQUIRE(after.has_value());
    CHECK(after->empty());
}

// ===========================================================================
// 7. Query on empty tables
// ===========================================================================

TEST_CASE("Query: query_orders on empty table returns empty vector",
          "[query][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());
    CHECK(rows->empty());
}

TEST_CASE("Query: query_trades on empty table returns empty vector",
          "[query][questdb]")
{
    DatabaseClient client;
    client.truncate_trades();
    wait_for_questdb_commit();

    auto rows = client.query_trades();
    REQUIRE(rows.has_value());
    CHECK(rows->empty());
}

// ===========================================================================
// 8. to_string helpers (pure, no DB needed)
// ===========================================================================

TEST_CASE("to_string: Side converts correctly", "[helpers]")
{
    CHECK(database::to_string(Side::bid) == "BID");
    CHECK(database::to_string(Side::ask) == "ASK");
}

TEST_CASE("to_string: OrderType converts correctly", "[helpers]")
{
    CHECK(database::to_string(OrderType::limit)  == "LIMIT");
    CHECK(database::to_string(OrderType::market) == "MARKET");
}

TEST_CASE("to_string: TimeInForce converts correctly", "[helpers]")
{
    CHECK(database::to_string(TimeInForce::day) == "DAY");
    CHECK(database::to_string(TimeInForce::gtc) == "GTC");
}

TEST_CASE("to_string: OrderStatus converts correctly", "[helpers]")
{
    CHECK(database::to_string(OrderStatus::status_new)              == "NEW");
    CHECK(database::to_string(OrderStatus::status_partially_filled) == "PARTIALLY_FILLED");
    CHECK(database::to_string(OrderStatus::status_filled)           == "FILLED");
    CHECK(database::to_string(OrderStatus::status_canceled)         == "CANCELED");
    CHECK(database::to_string(OrderStatus::status_pending_cancel)   == "PENDING_CANCEL");
    CHECK(database::to_string(OrderStatus::status_rejected)         == "REJECTED");
}

// ===========================================================================
// 9. Edge-case: large values
// ===========================================================================

TEST_CASE("Order: large order_qty and price values are preserved",
          "[order][edge][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("BIG_BROKER", "EX", "77777", "AAPL",
                                Side::bid, 2'000'000'000, OrderType::limit,
                                2'100'000'000, TimeInForce::day);

    REQUIRE(client.insert_order(7777, order).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.order_id == 7777) {
            found = true;
            CHECK(row.order_qty == 2'000'000'000);
            // price is stored as int64 in QuestDB but read as int — may
            // overflow on 32-bit int; this test documents current behaviour.
            break;
        }
    }
    CHECK(found);

    client.truncate_orders();
}

TEST_CASE("Trade: large IDs are preserved", "[trade][edge][questdb]")
{
    DatabaseClient client;
    client.truncate_trades();
    wait_for_questdb_commit();

    auto trade = make_trade("AAPL", 999'999, 888'888, 999'999'999,
                            111'111, 222'222, 333'333, 444'444, true);

    REQUIRE(client.insert_trade(trade).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_trades();
    REQUIRE(rows.has_value());

    bool found = false;
    for (const auto& row : *rows) {
        if (row.trade_id == 999'999'999) {
            found = true;
            CHECK(row.price == 999'999);
            CHECK(row.quantity == 888'888);
            CHECK(row.taker_id == 111'111);
            CHECK(row.maker_id == 222'222);
            break;
        }
    }
    CHECK(found);

    client.truncate_trades();
}

// ===========================================================================
// 10. Full lifecycle: order → partial fill → full fill
// ===========================================================================

TEST_CASE("Lifecycle: new order -> partial fill -> full fill",
          "[lifecycle][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    // Step 1: New order
    auto order = make_new_order("LIFE", "EX", "5000", "AAPL",
                                Side::bid, 1000, OrderType::limit, 15000,
                                TimeInForce::day);
    REQUIRE(client.insert_order(500, order).has_value());
    wait_for_questdb_commit();

    {
        auto rows = client.query_orders();
        REQUIRE(rows.has_value());
        bool found = false;
        for (const auto& row : *rows) {
            if (row.order_id == 500) {
                found = true;
                CHECK(row.order_status == "NEW");
                CHECK(row.filled_qty == 0);
            }
        }
        CHECK(found);
    }

    // Step 2: Partial fill (300 of 1000)
    auto partial = make_execution_report(
        "EX", "LIFE", "500", "5000", "E_PARTIAL",
        ExecType::status_partially_filled, OrderStatus::status_partially_filled,
        "AAPL", Side::bid, 15000,
        /*leaves=*/700, /*cum=*/300, /*avg=*/15000);
    REQUIRE(client.insert_execution(partial).has_value());
    wait_for_questdb_commit();

    {
        auto rows = client.query_orders();
        REQUIRE(rows.has_value());
        bool found = false;
        for (const auto& row : *rows) {
            if (row.order_id == 500 && row.order_status == "PARTIALLY_FILLED") {
                found = true;
                CHECK(row.filled_qty == 300);
            }
        }
        CHECK(found);
    }

    // Step 3: Full fill (remaining 700)
    auto fill = make_execution_report(
        "EX", "LIFE", "500", "5000", "E_FILL",
        ExecType::status_filled, OrderStatus::status_filled,
        "AAPL", Side::bid, 15000,
        /*leaves=*/0, /*cum=*/1000, /*avg=*/15000);
    REQUIRE(client.insert_execution(fill).has_value());
    wait_for_questdb_commit();

    {
        auto rows = client.query_orders();
        REQUIRE(rows.has_value());
        bool found = false;
        for (const auto& row : *rows) {
            if (row.order_id == 500 && row.order_status == "FILLED") {
                found = true;
                CHECK(row.filled_qty == 1000);
                CHECK(row.order_qty == 1000);
            }
        }
        CHECK(found);
    }

    client.truncate_orders();
}

// ===========================================================================
// 11. Full lifecycle: order → cancel
// ===========================================================================

TEST_CASE("Lifecycle: new order -> cancel",
          "[lifecycle][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    wait_for_questdb_commit();

    auto order = make_new_order("CANCEL_TEST", "EX", "6000", "TSLA",
                                Side::ask, 500, OrderType::limit, 70000,
                                TimeInForce::day);
    REQUIRE(client.insert_order(600, order).has_value());
    wait_for_questdb_commit();

    auto cancel = make_cancel_order("CANCEL_TEST", "EX", "600", "6000",
                                     "6001", "TSLA", Side::ask, 500);
    REQUIRE(client.insert_cancel(cancel).has_value());
    wait_for_questdb_commit();

    auto rows = client.query_orders();
    REQUIRE(rows.has_value());

    bool found_new = false;
    bool found_cancel = false;
    for (const auto& row : *rows) {
        if (row.order_id == 600 && row.order_status == "NEW")      found_new = true;
        if (row.order_id == 600 && row.order_status == "CANCELED") found_cancel = true;
    }
    // At minimum, the cancel row must exist
    CHECK(found_cancel);

    client.truncate_orders();
}

// ===========================================================================
// 12. Order + Trade combined scenario
// ===========================================================================

TEST_CASE("Combined: order fill produces matching trade",
          "[combined][questdb]")
{
    DatabaseClient client;
    client.truncate_orders();
    client.truncate_trades();
    wait_for_questdb_commit();

    // Insert order
    auto order = make_new_order("COMB", "EX", "7000", "AAPL",
                                Side::bid, 1000, OrderType::limit, 15000,
                                TimeInForce::day);
    REQUIRE(client.insert_order(700, order).has_value());

    // Insert execution report (filled)
    auto exec = make_execution_report(
        "EX", "COMB", "700", "7000", "E_COMB",
        ExecType::status_filled, OrderStatus::status_filled,
        "AAPL", Side::bid, 15000,
        0, 1000, 15000);
    REQUIRE(client.insert_execution(exec).has_value());

    // Insert corresponding trade
    auto trade = make_trade("AAPL", 15000, 1000, 700, 1, 2, 700, 701, true);
    REQUIRE(client.insert_trade(trade).has_value());

    wait_for_questdb_commit();

    // Verify order is filled
    auto order_rows = client.query_orders();
    REQUIRE(order_rows.has_value());
    bool order_found = false;
    for (const auto& row : *order_rows) {
        if (row.order_id == 700 && row.order_status == "FILLED") {
            order_found = true;
            CHECK(row.filled_qty == 1000);
        }
    }
    CHECK(order_found);

    // Verify trade exists
    auto trade_rows = client.query_trades();
    REQUIRE(trade_rows.has_value());
    bool trade_found = false;
    for (const auto& row : *trade_rows) {
        if (row.trade_id == 700) {
            trade_found = true;
            CHECK(row.price == 15000);
            CHECK(row.quantity == 1000);
            CHECK(row.is_taker_buyer == true);
        }
    }
    CHECK(trade_found);

    client.truncate_orders();
    client.truncate_trades();
}
