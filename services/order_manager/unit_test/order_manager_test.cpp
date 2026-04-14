#include "order_manager.h"
#include "transport/inbound_server.h"
#include "transport/messaging.h"
#include "transport/outbound_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace om;

using testing::_;
using testing::InSequence;
using testing::Invoke;
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
    OrderManager::UsernameToUserIdMapContainer username_user_id_map;
    NiceMock<MockDatabaseClient> mock_db;
};
using BalanceCheckerInitDeathTest = BalanceCheckerInitTest;

TEST_F(BalanceCheckerInitDeathTest, DeathOnEmptyBalanceList) {
    ON_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillByDefault(Return(std::vector<DbUserBalanceInfo>{}));

    EXPECT_DEATH(init_balance_checker(balance_checker, username_user_id_map, mock_db), "");
}

TEST_F(BalanceCheckerInitDeathTest, DeathOnDatabaseError) {
    ON_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillByDefault(Return(std::unexpected<std::string>{"db error"}));

    EXPECT_DEATH(init_balance_checker(balance_checker, username_user_id_map, mock_db), "");
}

TEST_F(BalanceCheckerInitTest, LoadsBalancesForSingleUser) {
    EXPECT_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillOnce(Return(std::vector<DbUserBalanceInfo>{{
            .user_id = 0,
            .username = "alice",
            .balances = {{.symbol = "USD", .balance = 1000}, {.symbol = "AAPL", .balance = 25}},
        }}));

    init_balance_checker(balance_checker, username_user_id_map, mock_db);

    EXPECT_TRUE(balance_checker.broker_id_exists("alice"));
    EXPECT_TRUE(balance_checker.broker_owns_ticker("alice", "USD"));
    EXPECT_TRUE(balance_checker.broker_owns_ticker("alice", "AAPL"));
    EXPECT_EQ(balance_checker.get_balance("alice", "USD"), 1000);
    EXPECT_EQ(balance_checker.get_balance("alice", "AAPL"), 25);

    EXPECT_EQ(username_user_id_map.at("alice"), 0);
}

TEST_F(BalanceCheckerInitTest, LoadsBalancesForMultipleUsersAndTickers) {
    EXPECT_CALL(mock_db, get_all_users_balances_for_server(_))
        .WillOnce(Return(std::vector<DbUserBalanceInfo>{
            {.user_id = 0,
             .username = "alice",
             .balances = {{.symbol = "USD", .balance = 1500}, {.symbol = "TSLA", .balance = 5}}},
            {.user_id = 1, .username = "bob", .balances = {{.symbol = "USD", .balance = 2500}}},
        }));

    init_balance_checker(balance_checker, username_user_id_map, mock_db);

    EXPECT_TRUE(balance_checker.broker_id_exists("alice"));
    EXPECT_TRUE(balance_checker.broker_id_exists("bob"));

    EXPECT_EQ(balance_checker.get_balance("alice", "USD"), 1500);
    EXPECT_EQ(balance_checker.get_balance("alice", "TSLA"), 5);
    EXPECT_EQ(balance_checker.get_balance("bob", "USD"), 2500);
    EXPECT_FALSE(balance_checker.broker_owns_ticker("bob", "TSLA"));

    EXPECT_EQ(username_user_id_map.at("alice"), 0);
    EXPECT_EQ(username_user_id_map.at("bob"), 1);
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

TEST_F(ConnectMatchingEngineDeathTest, NonPositiveTryAttempts) {
    EXPECT_DEATH(test_om.connect_matching_engine(TEST_HOST, TEST_PORT, 0), "");
}

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

class PreprocessContainerTest : public testing::Test {
  protected:
    OrderManager::OrderIdMapContainer order_id_map;
    OrderManager::OrderInfoMapContainer order_info_map;
    OrderManager::UsernameToUserIdMapContainer username_user_id_map;
    MockOutboundClient mock_order_request_client;

    PreprocessContainerTest() {
        username_user_id_map.emplace("CLIENT", 1);
    }
};

TEST_F(PreprocessContainerTest, NewOrderUnknownSender) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "STRANGER",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    preprocess_container(new_order, order_id_map, order_info_map, username_user_id_map, 0,
                         mock_order_request_client, 0)
        .transform([](std::optional<int>) { ADD_FAILURE(); })
        .transform_error([](std::string&& err) -> std::string {
            // TODO: Check correct error after error enums are established
            SUCCEED();
            return err;
        });
}

TEST_F(PreprocessContainerTest, NewOrderDuplicateClOrdId) {
    core::Container new_order_1 =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    core::Container new_order_2 =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    preprocess_container(new_order_1, order_id_map, order_info_map, username_user_id_map, 0,
                         mock_order_request_client, 0)
        .transform([&](std::optional<int>) {
            EXPECT_EQ(order_id_map.left.at(
                          std::get<core::NewOrderSingleContainer>(new_order_1).order_id.value()),
                      100 * core::constants::max_user_count + 1);
        })
        .transform_error([](std::string&& err) -> std::string {
            ADD_FAILURE();
            return err;
        });

    preprocess_container(new_order_2, order_id_map, order_info_map, username_user_id_map, 0,
                         mock_order_request_client, 0)
        .transform([](std::optional<int>) { ADD_FAILURE(); })
        .transform_error([](std::string&& err) -> std::string {
            SUCCEED();
            return err;
        });
}

TEST_F(PreprocessContainerTest, NonMarketBidNewOrder) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    std::ignore = preprocess_container(new_order, order_id_map, order_info_map,
                                       username_user_id_map, 0, mock_order_request_client, 0);

    std::ignore =
        std::get<core::NewOrderSingleContainer>(new_order)
            .order_id
            .transform([&](int order_id) {
                // Assume "CLIENT" has internal user id of 1
                constexpr auto expected_transformed_cl_ord_id =
                    100 * core::constants::max_user_count + 1;
                EXPECT_EQ(order_id_map.left.at(order_id), expected_transformed_cl_ord_id);
                EXPECT_EQ(order_id_map.right.at(expected_transformed_cl_ord_id), order_id);

                const auto order_info = order_info_map.at(order_id);
                EXPECT_EQ(order_info.sender_comp_id, "CLIENT");
                EXPECT_EQ(order_info.symbol, "AAPL");
                EXPECT_EQ(order_info.side, core::Side::ask);
                EXPECT_EQ(order_info.price, 100);
                EXPECT_EQ(order_info.time_in_force, core::TimeInForce::gtc);
                EXPECT_EQ(order_info.leaves_qty, 10);
                EXPECT_EQ(order_info.cum_qty, 0);
                EXPECT_EQ(order_info.avg_px, 0);
                EXPECT_EQ(order_info.arrival_gateway_id, 0);
                return order_id;
            })
            .or_else([] -> std::optional<int> {
                ADD_FAILURE();
                return std::nullopt;
            });
}

TEST_F(PreprocessContainerTest, MarketBidNewOrder) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = std::nullopt,
                                      .time_in_force = core::TimeInForce::gtc};

    EXPECT_CALL(mock_order_request_client, send)
        .WillOnce(Invoke([](int, const std::string& payload,
                            transport::MessageFormat) -> std::expected<void, int> {
            const auto container = transport::deserialize_container((payload));
            auto query = std::get_if<core::FillCostQueryContainer>(&container);

            EXPECT_NE(query, nullptr);
            if (query == nullptr) {
                return std::unexpected{-1};
            }

            EXPECT_EQ(query->symbol, "AAPL");
            EXPECT_EQ(query->quantity, 10);
            EXPECT_EQ(query->side, core::Side::ask);

            return std::expected<void, int>{};
        }));

    EXPECT_CALL(mock_order_request_client, wait_and_dequeue_message(0))
        .WillOnce(Return(
            transport::serialize_container(core::FillCostResponseContainer{.total_cost = 1000})));

    preprocess_container(new_order, order_id_map, order_info_map, username_user_id_map, 0,
                         mock_order_request_client, 0)
        .transform([](std::optional<int> fill_cost) {
            EXPECT_EQ(fill_cost.value(), 1000);
            return fill_cost;
        })
        .transform_error([](std::string&& err) -> std::string {
            ADD_FAILURE();
            return err;
        });

    std::ignore =
        std::get<core::NewOrderSingleContainer>(new_order)
            .order_id
            .transform([&](int order_id) {
                // Assume "CLIENT" has internal user id of 1
                const auto expected_transformed_cl_ord_id =
                    100 * core::constants::max_user_count + 1;
                EXPECT_EQ(order_id_map.left.at(order_id), expected_transformed_cl_ord_id);
                EXPECT_EQ(order_id_map.right.at(expected_transformed_cl_ord_id), order_id);

                const auto order_info = order_info_map.at(order_id);
                EXPECT_EQ(order_info.sender_comp_id, "CLIENT");
                EXPECT_EQ(order_info.symbol, "AAPL");
                EXPECT_EQ(order_info.side, core::Side::bid);
                EXPECT_EQ(order_info.price, std::nullopt);
                EXPECT_EQ(order_info.time_in_force, core::TimeInForce::gtc);
                EXPECT_EQ(order_info.leaves_qty, 10);
                EXPECT_EQ(order_info.cum_qty, 0);
                EXPECT_EQ(order_info.avg_px, 0);
                EXPECT_EQ(order_info.arrival_gateway_id, 0);
                return order_id;
            })
            .or_else([] -> std::optional<int> {
                ADD_FAILURE();
                return std::nullopt;
            });
}

TEST_F(PreprocessContainerTest, MarketBidNewOrderFillCostQueryFail) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = std::nullopt,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = std::nullopt,
                                      .time_in_force = core::TimeInForce::gtc};

    EXPECT_CALL(mock_order_request_client, send).WillOnce(Return(std::unexpected{-1}));

    std::ignore = preprocess_container(new_order, order_id_map, order_info_map,
                                       username_user_id_map, 0, mock_order_request_client, 0)
                      .transform([](std::optional<int>) { ADD_FAILURE(); })
                      .transform_error([](std::string&& err) -> std::string {
                          SUCCEED();
                          return err;
                      });
}

TEST_F(PreprocessContainerTest, CancelRequestUnknownSender) {
    core::Container cancel_request = core::CancelOrderRequestContainer{.sender_comp_id = "STRANGER",
                                                                       .target_comp_id = "OM",
                                                                       .order_id = std::nullopt,
                                                                       .orig_cl_ord_id = 100,
                                                                       .cl_ord_id = 1234,
                                                                       .symbol = "AAPL",
                                                                       .side = core::Side::bid,
                                                                       .order_qty = 10};

    std::ignore = preprocess_container(cancel_request, order_id_map, order_info_map,
                                       username_user_id_map, 0, mock_order_request_client, 0)
                      .transform([](std::optional<int>) { ADD_FAILURE(); })
                      .transform_error([](std::string&& err) -> std::string {
                          // TODO: Check correct error after error enums are established
                          SUCCEED();
                          return err;
                      });
}

TEST_F(PreprocessContainerTest, ValidCancelRequest) {
    core::Container cancel_request = core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                                                       .target_comp_id = "OM",
                                                                       .order_id = std::nullopt,
                                                                       .orig_cl_ord_id = 100,
                                                                       .cl_ord_id = 1234,
                                                                       .symbol = "AAPL",
                                                                       .side = core::Side::bid,
                                                                       .order_qty = 10};

    order_id_map.insert(OrderManager::OrderIdPair(0, 100 * core::constants::max_user_count + 1));
    username_user_id_map.emplace("CLIENT", 1);

    std::ignore = preprocess_container(cancel_request, order_id_map, order_info_map,
                                       username_user_id_map, 0, mock_order_request_client, 0);

    std::get<core::CancelOrderRequestContainer>(cancel_request)
        .order_id
        .transform([&](int oid) {
            EXPECT_EQ(oid, 0);
            return oid;
        })
        .or_else([] -> std::optional<int> {
            ADD_FAILURE();
            return std::nullopt;
        });
}

TEST_F(PreprocessContainerTest, InvalidCancelRequest) {
    core::Container cancel_request = core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                                                       .target_comp_id = "OM",
                                                                       .order_id = std::nullopt,
                                                                       .orig_cl_ord_id = 100,
                                                                       .cl_ord_id = 1234,
                                                                       .symbol = "AAPL",
                                                                       .side = core::Side::bid,
                                                                       .order_qty = 10};

    username_user_id_map.emplace("CLIENT", 1);

    std::ignore = preprocess_container(cancel_request, order_id_map, order_info_map,
                                       username_user_id_map, 0, mock_order_request_client, 0);

    std::get<core::CancelOrderRequestContainer>(cancel_request)
        .order_id
        .transform([&](int oid) {
            ADD_FAILURE();
            return oid;
        })
        .or_else([] -> std::optional<int> {
            SUCCEED();
            return std::nullopt;
        });
}

class ValidateContainerTest : public testing::Test {
  protected:
    BalanceChecker balance_checker;
};
using ValidateContainerDeathTest = ValidateContainerTest;

TEST_F(ValidateContainerTest, ValidLimitBid) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", USD_SYMBOL, 1000);

    EXPECT_EQ(validate_container(new_order, balance_checker), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "USD"), 0);
}

TEST_F(ValidateContainerTest, InvalidLimitBid) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", USD_SYMBOL, 10);

    EXPECT_NE(validate_container(new_order, balance_checker), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "USD"), 10);
}

TEST_F(ValidateContainerTest, ValidMarketBid) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", USD_SYMBOL, 1000);

    EXPECT_EQ(validate_container(new_order, balance_checker, 1000), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "USD"), 0);
}

TEST_F(ValidateContainerTest, InvalidMarketBid) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", USD_SYMBOL, 1000);

    EXPECT_NE(validate_container(new_order, balance_checker, 100000), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "USD"), 1000);
}

TEST_F(ValidateContainerTest, MissingFillCost) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::bid,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", USD_SYMBOL, 1000);

    EXPECT_NE(validate_container(new_order, balance_checker, std::nullopt), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "USD"), 1000);
}

TEST_F(ValidateContainerTest, ValidLimitAsk) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", "AAPL", 10);

    EXPECT_EQ(validate_container(new_order, balance_checker), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "AAPL"), 0);
}

TEST_F(ValidateContainerTest, InvalidLimitAsk) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", "AAPL", 1);

    EXPECT_NE(validate_container(new_order, balance_checker), "ok");
    EXPECT_EQ(balance_checker.get_balance("CLIENT", "AAPL"), 1);
}

TEST_F(ValidateContainerTest, ValidMarketAsk) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", "AAPL", 10);

    EXPECT_EQ(validate_container(new_order, balance_checker), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "AAPL"), 0);
}

TEST_F(ValidateContainerTest, InvalidMarketAsk) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::market,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    balance_checker.update_balance("CLIENT", "AAPL", 1);

    EXPECT_NE(validate_container(new_order, balance_checker), "ok");

    EXPECT_EQ(balance_checker.get_balance("CLIENT", "AAPL"), 1);
}

TEST_F(ValidateContainerTest, NoRecordInBalanceChecker) {
    constexpr core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 0,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    EXPECT_NE(validate_container(new_order, balance_checker), "ok");
}

TEST_F(ValidateContainerTest, ValidCancelRequest) {
    const core::Container cancel_request =
        core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                          .target_comp_id = "OM",
                                          .order_id = 0,
                                          .orig_cl_ord_id = 100,
                                          .cl_ord_id = 1234,
                                          .symbol = "AAPL",
                                          .side = core::Side::bid,
                                          .order_qty = 10};

    EXPECT_EQ(validate_container(cancel_request, balance_checker), "ok");
}

TEST_F(ValidateContainerTest, InvalidCancelRequest) {
    const core::Container cancel_request =
        core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                          .target_comp_id = "OM",
                                          .order_id = std::nullopt,
                                          .orig_cl_ord_id = 100,
                                          .cl_ord_id = 1234,
                                          .symbol = "AAPL",
                                          .side = core::Side::bid,
                                          .order_qty = 10};

    EXPECT_NE(validate_container(cancel_request, balance_checker), "ok");
}

class GenerateRejectionReportContainerTest : public testing::Test {
  protected:
    OrderManager::OrderInfoMapContainer order_info_map;
};

TEST_F(GenerateRejectionReportContainerTest, NewOrderRejectionBuildsExpectedExecutionReport) {
    const core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 123,
                                      .time_in_force = core::TimeInForce::gtc};

    const auto rejection =
        generate_rejection_report_container(new_order, order_info_map, "bad order state");

    EXPECT_EQ(rejection.sender_comp_id, SERVER_NAME);
    EXPECT_EQ(rejection.target_comp_id, "CLIENT");
    EXPECT_EQ(rejection.order_id, 42);
    EXPECT_EQ(rejection.cl_order_id, 100);
    EXPECT_EQ(rejection.orig_cl_ord_id, std::nullopt);
    EXPECT_EQ(rejection.exec_trans_type, core::ExecTransType::exec_trans_new);
    EXPECT_EQ(rejection.exec_type, core::ExecType::status_rejected);
    EXPECT_EQ(rejection.ord_status, core::OrderStatus::status_rejected);
    EXPECT_EQ(rejection.text, "bad order state");
    EXPECT_EQ(rejection.symbol, "AAPL");
    EXPECT_EQ(rejection.side, core::Side::ask);
    EXPECT_EQ(rejection.price, 123);
    EXPECT_EQ(rejection.time_in_force, core::TimeInForce::gtc);
    EXPECT_EQ(rejection.leaves_qty, 0);
    EXPECT_EQ(rejection.cum_qty, 0);
    EXPECT_EQ(rejection.avg_px, 0);
    EXPECT_FALSE(rejection.exec_id.empty());
}

TEST_F(GenerateRejectionReportContainerTest,
       CancelRequestRejectionWithoutOrderIdUsesFallbackValues) {
    const core::Container cancel_request =
        core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                          .target_comp_id = "OM",
                                          .order_id = std::nullopt,
                                          .orig_cl_ord_id = 101,
                                          .cl_ord_id = 102,
                                          .symbol = "AAPL",
                                          .side = core::Side::ask,
                                          .order_qty = 9};

    const auto rejection = generate_rejection_report_container(
        cancel_request, order_info_map, "Cancel request original client order ID not found");

    EXPECT_EQ(rejection.sender_comp_id, SERVER_NAME);
    EXPECT_EQ(rejection.target_comp_id, "CLIENT");
    EXPECT_EQ(rejection.order_id, -1);
    EXPECT_EQ(rejection.cl_order_id, 102);
    EXPECT_EQ(rejection.orig_cl_ord_id, 101);
    EXPECT_EQ(rejection.exec_type, core::ExecType::status_rejected);
    EXPECT_EQ(rejection.ord_status, core::OrderStatus::status_rejected);
    EXPECT_EQ(rejection.text, "Cancel request original client order ID not found");
    EXPECT_EQ(rejection.symbol, "");
    EXPECT_EQ(rejection.price, std::nullopt);
    EXPECT_EQ(rejection.leaves_qty, 0);
    EXPECT_EQ(rejection.cum_qty, 0);
    EXPECT_EQ(rejection.avg_px, 0);
}

class GenerateSuccessReportContainerTest : public testing::Test {
  protected:
    OrderManager::OrderInfoMapContainer order_info_map;
};

TEST_F(GenerateSuccessReportContainerTest, NewOrderSuccessBuildsExpectedExecutionReport) {
    const core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 123,
                                      .time_in_force = core::TimeInForce::gtc};

    const auto success = generate_success_report_container(new_order, order_info_map);

    EXPECT_EQ(success.sender_comp_id, SERVER_NAME);
    EXPECT_EQ(success.target_comp_id, "CLIENT");
    EXPECT_EQ(success.order_id, 42);
    EXPECT_EQ(success.cl_order_id, 100);
    EXPECT_EQ(success.orig_cl_ord_id, std::nullopt);
    EXPECT_EQ(success.exec_trans_type, core::ExecTransType::exec_trans_new);
    EXPECT_EQ(success.exec_type, core::ExecType::status_new);
    EXPECT_EQ(success.ord_status, core::OrderStatus::status_new);
    EXPECT_EQ(success.text, std::nullopt);
    EXPECT_EQ(success.symbol, "AAPL");
    EXPECT_EQ(success.side, core::Side::ask);
    EXPECT_EQ(success.price, 123);
    EXPECT_EQ(success.time_in_force, core::TimeInForce::gtc);
    EXPECT_EQ(success.leaves_qty, 10);
    EXPECT_EQ(success.cum_qty, 0);
    EXPECT_EQ(success.avg_px, 0);
    EXPECT_FALSE(success.exec_id.empty());
}

TEST_F(GenerateSuccessReportContainerTest,
       CancelRequestSuccessUsesOriginalOrderInfoForExecutionReport) {
    order_info_map.emplace(42, OrderInfo{.sender_comp_id = "CLIENT",
                                         .symbol = "AAPL",
                                         .side = core::Side::bid,
                                         .price = 124,
                                         .time_in_force = core::TimeInForce::gtc,
                                         .leaves_qty = 7,
                                         .cum_qty = 3,
                                         .avg_px = 125,
                                         .arrival_gateway_id = 0});

    const core::Container cancel_request =
        core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                          .target_comp_id = "OM",
                                          .order_id = 42,
                                          .orig_cl_ord_id = 101,
                                          .cl_ord_id = 102,
                                          .symbol = "AAPL",
                                          .side = core::Side::ask,
                                          .order_qty = 9};

    const auto success = generate_success_report_container(cancel_request, order_info_map);

    EXPECT_EQ(success.sender_comp_id, SERVER_NAME);
    EXPECT_EQ(success.target_comp_id, "CLIENT");
    EXPECT_EQ(success.order_id, 42);
    EXPECT_EQ(success.cl_order_id, 102);
    EXPECT_EQ(success.orig_cl_ord_id, 101);
    EXPECT_EQ(success.exec_trans_type, core::ExecTransType::exec_trans_new);
    EXPECT_EQ(success.exec_type, core::ExecType::status_pending_cancel);
    EXPECT_EQ(success.ord_status, core::OrderStatus::status_pending_cancel);
    EXPECT_EQ(success.text, std::nullopt);
    EXPECT_EQ(success.symbol, "AAPL");
    EXPECT_EQ(success.side, core::Side::bid);
    EXPECT_EQ(success.price, 124);
    EXPECT_EQ(success.time_in_force, core::TimeInForce::gtc);
    EXPECT_EQ(success.leaves_qty, 7);
    EXPECT_EQ(success.cum_qty, 3);
    EXPECT_EQ(success.avg_px, 125);
    EXPECT_FALSE(success.exec_id.empty());
}

class ForwardAndReplyTest : public testing::Test {
  protected:
    MockOutboundClient mock_order_request_client;
    MockInboundServer mock_inbound_server;
    OrderManager::OrderInfoMapContainer order_info_map;
    int arrival_gateway_id = 0;
    int order_request_connection_id = 11;
};

TEST_F(ForwardAndReplyTest, ValidContainerForwardsAndRepliesWithSuccess) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    {
        InSequence seq;
        EXPECT_CALL(mock_order_request_client, send(order_request_connection_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
        EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
    }

    forward_and_reply(true, new_order, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server);
}

TEST_F(ForwardAndReplyTest, ValidContainerForwardFails) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    {
        InSequence seq;
        EXPECT_CALL(mock_order_request_client, send(order_request_connection_id, _, _))
            .WillOnce(Return(std::unexpected{-1}));
        // Should still attempt to send the success reply despite forward failure
        EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
    }

    forward_and_reply(true, new_order, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server);
}

TEST_F(ForwardAndReplyTest, ValidContainerReplyFails) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    {
        InSequence seq;
        EXPECT_CALL(mock_order_request_client, send(order_request_connection_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
        EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
            .WillOnce(Return(std::unexpected{-1}));
    }

    forward_and_reply(true, new_order, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server);
}

TEST_F(ForwardAndReplyTest, InvalidContainerRepliesWithRejection) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    // Should NOT call forward when container is invalid
    EXPECT_CALL(mock_order_request_client, send).Times(0);

    EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
        .WillOnce(Return(std::expected<void, int>{}));

    forward_and_reply(false, new_order, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server,
                      "Insufficient balance");
}

TEST_F(ForwardAndReplyTest, InvalidContainerReplyFails) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    // Should NOT call forward when container is invalid
    EXPECT_CALL(mock_order_request_client, send).Times(0);

    EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
        .WillOnce(Return(std::unexpected{-1}));

    forward_and_reply(false, new_order, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server,
                      "Unknown sender");
}

TEST_F(ForwardAndReplyTest, InvalidCancelRequestOrderNotFound) {
    // Cancel request with nullopt order_id (order was not found during preprocessing)
    core::Container cancel_request = core::CancelOrderRequestContainer{.sender_comp_id = "CLIENT",
                                                                       .target_comp_id = "OM",
                                                                       .order_id = std::nullopt,
                                                                       .orig_cl_ord_id = 100,
                                                                       .cl_ord_id = 101,
                                                                       .symbol = "AAPL",
                                                                       .side = core::Side::bid,
                                                                       .order_qty = 5};

    // No order_info_map entry since the order doesn't exist
    EXPECT_CALL(mock_order_request_client, send).Times(0);
    EXPECT_CALL(mock_inbound_server, send(arrival_gateway_id, _, _))
        .WillOnce(Return(std::expected<void, int>{}));

    forward_and_reply(false, cancel_request, order_info_map, arrival_gateway_id,
                      mock_order_request_client, order_request_connection_id, mock_inbound_server,
                      "Cancel request original client order ID not found");
}

TEST_F(ForwardAndReplyTest, ValidContainerWithMultipleGatewayIds) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 50,
                                      .cl_ord_id = 150,
                                      .symbol = "MSFT",
                                      .side = core::Side::ask,
                                      .order_qty = 20,
                                      .ord_type = core::OrderType::limit,
                                      .price = 300,
                                      .time_in_force = core::TimeInForce::gtc};

    const int gateway_id = 5;

    {
        InSequence seq;
        EXPECT_CALL(mock_order_request_client, send(order_request_connection_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
        EXPECT_CALL(mock_inbound_server, send(gateway_id, _, _))
            .WillOnce(Return(std::expected<void, int>{}));
    }

    forward_and_reply(true, new_order, order_info_map, gateway_id, mock_order_request_client,
                      order_request_connection_id, mock_inbound_server);
}

using ForwardAndReplyDeathTest = ForwardAndReplyTest;

TEST_F(ForwardAndReplyDeathTest, PreconditionViolationInvalidWithoutReason) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    // This violates the precondition: is_container_valid is false but no reason provided
    EXPECT_DEATH(forward_and_reply(false, new_order, order_info_map, arrival_gateway_id,
                                   mock_order_request_client, order_request_connection_id,
                                   mock_inbound_server),
                 "");
}

TEST_F(ForwardAndReplyDeathTest, PreconditionViolationValidWithReason) {
    core::Container new_order =
        core::NewOrderSingleContainer{.sender_comp_id = "CLIENT",
                                      .target_comp_id = "OM",
                                      .order_id = 42,
                                      .cl_ord_id = 100,
                                      .symbol = "AAPL",
                                      .side = core::Side::ask,
                                      .order_qty = 10,
                                      .ord_type = core::OrderType::limit,
                                      .price = 100,
                                      .time_in_force = core::TimeInForce::gtc};

    // This violates the precondition: is_container_valid is true but reason is provided
    EXPECT_DEATH(forward_and_reply(true, new_order, order_info_map, arrival_gateway_id,
                                   mock_order_request_client, order_request_connection_id,
                                   mock_inbound_server, "Should not provide reason"),
                 "");
}
