#include "order_manager.h"
#include "transport/inbound_server.h"
#include "transport/outbound_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace om;

using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;

const std::string TEST_HOST{"localhost"};
constexpr int TEST_PORT{1234};

class MockInboundServer : public transport::InboundServer {
  public:
    MOCK_METHOD((std::expected<void, int>), start, (), (override));
    MOCK_METHOD(std::vector<transport::InboundConnectionInfo>, get_connection_info, (),
                (const, override));
    MOCK_METHOD(std::optional<std::string>, dequeue_message, (int), (override));
    MOCK_METHOD((std::expected<void, int>), send,
                (int, const std::string&, transport::MessageFormat), (override));
};

class MockOutboundClient : public transport::OutboundClient {
  public:
    MOCK_METHOD((std::expected<void, int>), start, (), (override));
    MOCK_METHOD((std::expected<int, int>), connect, (std::string_view, std::string_view),
                (override));
    MOCK_METHOD(std::optional<std::string>, dequeue_message, (int), (override));
    MOCK_METHOD(std::optional<std::string>, wait_and_dequeue_message, (int), (override));
    MOCK_METHOD((std::expected<void, int>), send,
                (int, const std::string&, transport::MessageFormat), (override));
};

class MockDatabaseClient : public OrderManagerDatabase {
  public:
    MOCK_METHOD((std::expected<int, std::string>), ensure_initial_usd_balances,
                (std::string_view, int), (override));
    MOCK_METHOD((std::expected<std::vector<DbUserBalanceInfo>, std::string>),
                get_all_users_balances_for_server, (std::string_view), (override));
    MOCK_METHOD((std::expected<void, std::string>), insert_order,
                (int, const core::NewOrderSingleContainer&, bool), (override));
    MOCK_METHOD((std::expected<void, std::string>), insert_cancel_request,
                (const core::CancelOrderRequestContainer&, bool), (override));
    MOCK_METHOD((std::expected<void, std::string>), insert_trade, (const core::TradeContainer&),
                (override));
    MOCK_METHOD((std::expected<void, std::string>), insert_cancel_response,
                (const core::CancelOrderResponseContainer&), (override));
};

class BaseOrderManagerTest : public testing::Test {
  protected:
    MockInboundServer* mock_inbound_server;
    MockOutboundClient* mock_order_request_client;
    MockOutboundClient* mock_order_response_client;
    MockDatabaseClient* mock_database_client;
    OrderManagerDependencyFactory dependency_factory;
    OrderManager test_om;

    BaseOrderManagerTest()
        : mock_inbound_server(nullptr), mock_order_request_client(nullptr),
          mock_order_response_client(nullptr), mock_database_client(nullptr), test_om{[&] {
              dependency_factory.create_inbound_server = [this](std::string_view, int,
                                                                std::shared_ptr<spdlog::logger>,
                                                                std::vector<int>&) {
                  auto ws = std::make_unique<NiceMock<MockInboundServer>>();
                  mock_inbound_server = ws.get();
                  return ws;
              };
              dependency_factory.create_outbound_client = [this](std::shared_ptr<spdlog::logger>) {
                  auto ws = std::make_unique<NiceMock<MockOutboundClient>>();
                  if (creation_count == 0) {
                      mock_order_request_client = ws.get();
                  } else if (creation_count == 1) {
                      mock_order_response_client = ws.get();
                  } else {
                      ADD_FAILURE();
                  }

                  ++creation_count;
                  return ws;
              };
              dependency_factory.create_database_client = [this](bool) {
                  auto db = std::make_unique<MockDatabaseClient>();
                  mock_database_client = db.get();
                  return db;
              };
              return OrderManager(TEST_HOST, TEST_PORT, dependency_factory);
          }()} {
    }

  private:
    int creation_count = 0;
};

using OrderManagerInitTest = BaseOrderManagerTest;
using OrderManagerInitDeathTest = OrderManagerInitTest;

TEST_F(OrderManagerInitDeathTest, InboundServerStartFailure) {
    ON_CALL(*mock_inbound_server, start).WillByDefault(Return(std::unexpected{-1}));

    EXPECT_DEATH(test_om.init(), "");
}

TEST_F(OrderManagerInitDeathTest, OrderRequestClientStartFailure) {
    ON_CALL(*mock_order_request_client, start).WillByDefault(Return(std::unexpected{-1}));

    EXPECT_DEATH(test_om.init(), "");
}

TEST_F(OrderManagerInitDeathTest, OrderResponseClientStartFailure) {
    ON_CALL(*mock_order_response_client, start).WillByDefault(Return(std::unexpected{-1}));

    EXPECT_DEATH(test_om.init(), "");
}

class BalanceCheckerInitTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;
    NiceMock<MockDatabaseClient> mock_db;
};
using BalanceCheckerInitDeathTest = BalanceCheckerInitTest;

TEST_F(BalanceCheckerInitDeathTest, DeathOnEmptyBalanceList) {
    ON_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillByDefault(Return(std::vector<DbUserBalanceInfo>{}));

    EXPECT_DEATH(init_balance_checker(balance_checker, mock_db), "");
}

TEST_F(BalanceCheckerInitDeathTest, DeathOnDatabaseError) {
    ON_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillByDefault(Return(std::unexpected<std::string>{"db error"}));

    EXPECT_DEATH(init_balance_checker(balance_checker, mock_db), "");
}

TEST_F(BalanceCheckerInitTest, LoadsBalancesForSingleUser) {
    EXPECT_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillOnce(Return(std::vector<DbUserBalanceInfo>{{
            .username = "alice",
            .balances = {{.symbol = "USD", .balance = 1000}, {.symbol = "AAPL", .balance = 25}},
        }}));

    init_balance_checker(balance_checker, mock_db);

    EXPECT_TRUE(balance_checker.broker_id_exists("alice"));
    EXPECT_TRUE(balance_checker.broker_owns_ticker("alice", "USD"));
    EXPECT_TRUE(balance_checker.broker_owns_ticker("alice", "AAPL"));
    EXPECT_EQ(balance_checker.get_balance("alice", "USD"), 1000);
    EXPECT_EQ(balance_checker.get_balance("alice", "AAPL"), 25);
}

TEST_F(BalanceCheckerInitTest, LoadsBalancesForMultipleUsersAndTickers) {
    EXPECT_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillOnce(Return(std::vector<DbUserBalanceInfo>{
            {.username = "alice",
             .balances = {{.symbol = "USD", .balance = 1500}, {.symbol = "TSLA", .balance = 5}}},
            {.username = "bob", .balances = {{.symbol = "USD", .balance = 2500}}},
        }));

    init_balance_checker(balance_checker, mock_db);

    EXPECT_TRUE(balance_checker.broker_id_exists("alice"));
    EXPECT_TRUE(balance_checker.broker_id_exists("bob"));

    EXPECT_EQ(balance_checker.get_balance("alice", "USD"), 1500);
    EXPECT_EQ(balance_checker.get_balance("alice", "TSLA"), 5);
    EXPECT_EQ(balance_checker.get_balance("bob", "USD"), 2500);
    EXPECT_FALSE(balance_checker.broker_owns_ticker("bob", "TSLA"));
}

using ConnectMatchingEngineTest = BaseOrderManagerTest;

TEST_F(ConnectMatchingEngineTest, ConnectsBothMatchingEngineClientsOnFirstTry) {
    {
        InSequence seq;
        EXPECT_CALL(*mock_order_request_client, connect).WillOnce(Return(11));
        EXPECT_CALL(*mock_order_response_client, connect).WillOnce(Return(22));
    }

    test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 1);
}

TEST_F(ConnectMatchingEngineTest, RetriesOrderRequestConnectionUntilSuccessful) {
    {
        InSequence seq;
        EXPECT_CALL(*mock_order_request_client, connect)
            .WillOnce(Return(std::unexpected{-1}))
            .WillOnce(Return(11));
        EXPECT_CALL(*mock_order_response_client, connect).WillOnce(Return(22));
    }

    test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 2);
}

TEST_F(ConnectMatchingEngineTest, RetriesOrderResponseConnectionUntilSuccessful) {
    {
        InSequence seq;
        EXPECT_CALL(*mock_order_request_client, connect).WillOnce(Return(11));
        EXPECT_CALL(*mock_order_response_client, connect)
            .WillOnce(Return(std::unexpected{-2}))
            .WillOnce(Return(22));
    }

    test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 2);
}

using ConnectMatchingEngineDeathTest = ConnectMatchingEngineTest;

TEST_F(ConnectMatchingEngineDeathTest, DeathOnOrderRequestConnectionExhaustion) {
    EXPECT_DEATH(
        {
            EXPECT_CALL(*mock_order_request_client, connect)
                .WillRepeatedly(Return(std::unexpected{-1}));

            test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 2);
        },
        "");
}

TEST_F(ConnectMatchingEngineDeathTest, DeathOnOrderResponseConnectionExhaustion) {
    EXPECT_DEATH(
        {
            {
                InSequence seq;
                EXPECT_CALL(*mock_order_request_client, connect).WillOnce(Return(0));
                EXPECT_CALL(*mock_order_response_client, connect)
                    .WillRepeatedly(Return(std::unexpected{-1}));
            }

            test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 2);
        },
        "");
}
