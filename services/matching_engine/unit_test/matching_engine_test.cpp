#include "matching_engine.h"
#include "transport/messaging.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace engine;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

const std::string TEST_HOST{"here"};
constexpr int TEST_PORT{1234};
const std::vector<std::string> TEST_SYMBOLS{"AAPL", "GME", "TSLA"};
constexpr std::chrono::milliseconds TEST_FLUSH_INTERVAL{1000};

class StubTradePublisher : public Publisher<Trade> {
  public:
    bool try_publish(Trade& trade) override {
        return true;
    }
};

class StubSnapshotPublisher : public Publisher<TopOrderBookLevelAggregates> {
  public:
    bool try_publish(TopOrderBookLevelAggregates& snapshot) override {
        return true;
    }
};

class MockInboundWebsocketServer : public InboundServer {
  public:
    MOCK_METHOD((std::expected<void, int>), start, (), (override));
    MOCK_METHOD(std::vector<InboundConnectionInfo>, get_connection_info, (), (const, override));
    MOCK_METHOD(std::optional<std::string>, dequeue_message, (int), (override));
    MOCK_METHOD((std::expected<void, int>), send, (int, const std::string&, InboundMessageFormat),
                (override));
};

MatchingEngineDependencyFactory make_base_test_dependency_factory() {
    return MatchingEngineDependencyFactory{
        .create_trade_publisher =
            [](std::string_view) { return std::make_unique<StubTradePublisher>(); },
        .create_orderbook_snapshot_publisher =
            [](std::string_view) { return std::make_unique<StubSnapshotPublisher>(); }};
};

TEST(MatchingEngineConstructorTest, ValidConstruction) {
    auto dependency_factory = make_base_test_dependency_factory();
    dependency_factory.create_inbound_server = [](std::string_view, int,
                                                  std::shared_ptr<spdlog::logger>) {
        return std::make_unique<MockInboundWebsocketServer>();
    };

    MatchingEngine test_matching_engine{TEST_HOST, TEST_PORT, TEST_SYMBOLS, TEST_FLUSH_INTERVAL,
                                        dependency_factory};

    const auto& limit_order_books = test_matching_engine.get_limit_order_books();
    EXPECT_EQ(limit_order_books.size(), TEST_SYMBOLS.size());
    for (const auto& [symbol, lob] : limit_order_books) {
        EXPECT_EQ(symbol, lob.get_ticker());
    }

    const auto& snapshot_publishers = test_matching_engine.get_snapshot_publishers();
    EXPECT_EQ(snapshot_publishers.size(), TEST_SYMBOLS.size());
}

class MatchingEngineInitTest : public testing::Test {
  protected:
    NiceMock<MockInboundWebsocketServer>* mock_ws = nullptr;
    MatchingEngineDependencyFactory dep_factory;
    MatchingEngine test_me;

    MatchingEngineInitTest()
        : dep_factory{make_base_test_dependency_factory()}, test_me{[&]() {
              dep_factory.create_inbound_server = [this](std::string_view, int,
                                                         std::shared_ptr<spdlog::logger>) {
                  auto ws = std::make_unique<NiceMock<MockInboundWebsocketServer>>();
                  mock_ws = ws.get();
                  return ws;
              };
              return MatchingEngine{TEST_HOST, TEST_PORT, TEST_SYMBOLS, TEST_FLUSH_INTERVAL,
                                    dep_factory};
          }()} {
    }
};
using MatchingEngineInitDeathTest = MatchingEngineInitTest;

TEST_F(MatchingEngineInitDeathTest, WebsocketStartUpFailure) {
    ON_CALL(*mock_ws, start).WillByDefault(Return(std::unexpected{-1}));

    EXPECT_DEATH(test_me.init(), "");
}

TEST_F(MatchingEngineInitTest, WebsocketStartupSuccess) {
    EXPECT_CALL(*mock_ws, start).WillOnce(Return(std::expected<void, int>{}));

    test_me.init();
}

class WaitConnectionTest : public testing::Test {
  protected:
    NiceMock<MockInboundWebsocketServer>* mock_ws = nullptr;
    MatchingEngineDependencyFactory dep_factory;
    MatchingEngine test_me;

    WaitConnectionTest()
        : dep_factory{make_base_test_dependency_factory()}, test_me{[&]() {
              dep_factory.create_inbound_server = [this](std::string_view, int,
                                                         std::shared_ptr<spdlog::logger>) {
                  auto ws = std::make_unique<NiceMock<MockInboundWebsocketServer>>();
                  mock_ws = ws.get();
                  return ws;
              };
              return MatchingEngine{TEST_HOST, TEST_PORT, TEST_SYMBOLS, TEST_FLUSH_INTERVAL,
                                    dep_factory};
          }()} {
    }
};

TEST_F(WaitConnectionTest, ConnectionSuccess) {
    ON_CALL(*mock_ws, get_connection_info)
        .WillByDefault(Return(std::vector{InboundConnectionInfo{0, "order_request"},
                                          InboundConnectionInfo{1, "order_response"}}));

    test_me.wait_for_connections();
}

class ProcessContainerTest : public testing::Test {
  protected:
    ProcessContainerTest() {
        for (const auto& symbol : TEST_SYMBOLS) {
            test_limit_order_books.emplace(
                symbol,
                LimitOrderBook{symbol, trade_events, std::make_unique<StubTradePublisher>()});
        }
    }

    std::unordered_map<std::string, LimitOrderBook> test_limit_order_books;
    std::queue<Trade> trade_events;
    MockInboundWebsocketServer mock_ws;
};
using ProcessContainerDeathTest = ProcessContainerTest;

TEST_F(ProcessContainerDeathTest, InvalidNewOrder) {
    core::NewOrderSingleContainer invalid_order{.order_id = std::nullopt};

    EXPECT_DEATH(
        process_container(invalid_order, test_limit_order_books, trade_events, mock_ws, 0, 1), "");
}

TEST_F(ProcessContainerDeathTest, InvalidCancelRequest) {
    core::CancelOrderRequestContainer invalid_request{.order_id = std::nullopt};

    EXPECT_DEATH(
        process_container(invalid_request, test_limit_order_books, trade_events, mock_ws, 0, 1),
        "");
}

TEST_F(ProcessContainerTest, NewOrderAddedNoMatch) {
    core::NewOrderSingleContainer new_order{.sender_comp_id = "CLIENT",
                                            .target_comp_id = "ME",
                                            .order_id = 42,
                                            .cl_ord_id = 1001,
                                            .symbol = "AAPL",
                                            .side = Side::bid,
                                            .order_qty = 10,
                                            .ord_type = core::OrderType::limit,
                                            .price = 150,
                                            .time_in_force = core::TimeInForce::day};

    EXPECT_CALL(mock_ws, send).Times(0);

    process_container(new_order, test_limit_order_books, trade_events, mock_ws, 0, 1);

    const auto& lob = test_limit_order_books.at("AAPL");
    ASSERT_TRUE(lob.order_id_exists(42));

    EXPECT_TRUE(trade_events.empty());
}

TEST_F(ProcessContainerTest, NewOrderMatchSendsTradeContainer) {
    auto& lob = test_limit_order_books.at("AAPL");
    lob.add_order(1, 100, 5, Side::ask, "MAKER");

    core::NewOrderSingleContainer incoming_bid{.sender_comp_id = "CLIENT",
                                               .target_comp_id = "ME",
                                               .order_id = 2,
                                               .cl_ord_id = 1002,
                                               .symbol = "AAPL",
                                               .side = Side::bid,
                                               .order_qty = 5,
                                               .ord_type = core::OrderType::limit,
                                               .price = 100,
                                               .time_in_force = core::TimeInForce::day};

    EXPECT_CALL(mock_ws, send(0, _, InboundMessageFormat::binary))
        .WillOnce(Invoke(
            [](int, const std::string& payload, InboundMessageFormat) -> std::expected<void, int> {
                const auto container = transport::deserialize_container(payload);
                auto trade = std::get_if<core::TradeContainer>(&container);
                EXPECT_NE(trade, nullptr);
                if (trade == nullptr) {
                    return std::unexpected{-1};
                }

                EXPECT_EQ(trade->ticker, "AAPL");
                EXPECT_EQ(trade->price, 100);
                EXPECT_EQ(trade->quantity, 5);
                EXPECT_EQ(trade->taker_id, "CLIENT");
                EXPECT_EQ(trade->maker_id, "MAKER");
                EXPECT_EQ(trade->taker_order_id, 2);
                EXPECT_EQ(trade->maker_order_id, 1);
                EXPECT_TRUE(trade->is_taker_buyer);
                EXPECT_FALSE(trade->trade_id.empty());

                return std::expected<void, int>{};
            }));

    process_container(incoming_bid, test_limit_order_books, trade_events, mock_ws, 0, 1);

    EXPECT_FALSE(lob.order_id_exists(1));
    EXPECT_FALSE(lob.order_id_exists(2));

    EXPECT_TRUE(trade_events.empty());
}

TEST_F(ProcessContainerTest, TradeContainerSendFail) {
    auto& lob = test_limit_order_books.at("AAPL");
    lob.add_order(1, 100, 5, Side::ask, "MAKER");

    core::NewOrderSingleContainer incoming_bid{.sender_comp_id = "CLIENT",
                                               .target_comp_id = "ME",
                                               .order_id = 2,
                                               .cl_ord_id = 1002,
                                               .symbol = "AAPL",
                                               .side = Side::bid,
                                               .order_qty = 5,
                                               .ord_type = core::OrderType::limit,
                                               .price = 100,
                                               .time_in_force = core::TimeInForce::day};

    EXPECT_CALL(mock_ws, send).WillOnce(Return(std::unexpected{-1}));

    process_container(incoming_bid, test_limit_order_books, trade_events, mock_ws, 0, 1);

    EXPECT_EQ(trade_events.size(), 1);
}

TEST_F(ProcessContainerTest, SuccessfulCancel) {
    auto& lob = test_limit_order_books.at("AAPL");
    lob.add_order(1, 100, 5, Side::ask, "MAKER");

    core::CancelOrderRequestContainer cancel_request{.sender_comp_id = "CLIENT",
                                                     .target_comp_id = "ME",
                                                     .order_id = 1,
                                                     .orig_cl_ord_id = 1000,
                                                     .cl_ord_id = 1001,
                                                     .symbol = "AAPL",
                                                     .side = Side::ask,
                                                     .order_qty = 5};

    EXPECT_CALL(mock_ws, send(0, _, InboundMessageFormat::binary))
        .WillOnce(Invoke(
            [](int, const std::string& payload, InboundMessageFormat) -> std::expected<void, int> {
                const auto container = transport::deserialize_container(payload);
                auto cancel_response = std::get_if<core::CancelOrderResponseContainer>(&container);
                EXPECT_NE(cancel_response, nullptr);
                if (cancel_response == nullptr) {
                    return std::unexpected{-1};
                }

                EXPECT_EQ(cancel_response->order_id, 1);
                EXPECT_EQ(cancel_response->cl_ord_id, 1001);
                EXPECT_TRUE(cancel_response->success);
                return std::expected<void, int>{};
            }));

    process_container(cancel_request, test_limit_order_books, trade_events, mock_ws, 0, 1);

    EXPECT_FALSE(lob.order_id_exists(1));
}

TEST_F(ProcessContainerTest, FailedCancel) {
    core::CancelOrderRequestContainer cancel_request{.sender_comp_id = "CLIENT",
                                                     .target_comp_id = "ME",
                                                     .order_id = 1,
                                                     .orig_cl_ord_id = 1000,
                                                     .cl_ord_id = 1001,
                                                     .symbol = "AAPL",
                                                     .side = Side::ask,
                                                     .order_qty = 5};

    EXPECT_CALL(mock_ws, send(0, _, InboundMessageFormat::binary))
        .WillOnce(Invoke(
            [](int, const std::string& payload, InboundMessageFormat) -> std::expected<void, int> {
                const auto container = transport::deserialize_container(payload);
                auto cancel_response = std::get_if<core::CancelOrderResponseContainer>(&container);
                EXPECT_NE(cancel_response, nullptr);
                if (cancel_response == nullptr) {
                    return std::unexpected{-1};
                }

                EXPECT_EQ(cancel_response->order_id, 1);
                EXPECT_EQ(cancel_response->cl_ord_id, 1001);
                EXPECT_FALSE(cancel_response->success);
                return std::expected<void, int>{};
            }));

    process_container(cancel_request, test_limit_order_books, trade_events, mock_ws, 0, 1);
}

TEST_F(ProcessContainerTest, FillCostQueryReturnsComputedCost) {
    auto& lob = test_limit_order_books.at("AAPL");
    lob.add_order(1, 100, 10, Side::ask, "MAKER_1");
    lob.add_order(2, 101, 20, Side::ask, "MAKER_2");

    core::FillCostQueryContainer fill_cost_query{
        .symbol = "AAPL", .quantity = 25, .side = Side::ask};

    EXPECT_CALL(mock_ws, send(1, _, InboundMessageFormat::binary))
        .WillOnce(Invoke(
            [](int, const std::string& payload, InboundMessageFormat) -> std::expected<void, int> {
                const auto container = transport::deserialize_container(payload);
                auto response = std::get_if<core::FillCostResponseContainer>(&container);
                EXPECT_NE(response, nullptr);
                if (response == nullptr) {
                    return std::unexpected{-1};
                }

                std::ignore = response->total_cost
                                  .transform([](int tc) {
                                      EXPECT_EQ(tc, 2515); // 10*100 + 15*101
                                      return tc;
                                  })
                                  .or_else([]() -> std::optional<int> {
                                      ADD_FAILURE();
                                      return std::nullopt;
                                  });

                return std::expected<void, int>{};
            }));

    process_container(fill_cost_query, test_limit_order_books, trade_events, mock_ws, 0, 1);
}

TEST_F(ProcessContainerTest, FillCostQueryWhenNoLiquidity) {
    core::FillCostQueryContainer fill_cost_query{
        .symbol = "AAPL",
        .quantity = 10,
        .side = Side::ask,
    };

    EXPECT_CALL(mock_ws, send(1, _, InboundMessageFormat::binary))
        .WillOnce(Invoke(
            [](int, const std::string& payload, InboundMessageFormat) -> std::expected<void, int> {
                const auto container = transport::deserialize_container(payload);
                auto response = std::get_if<core::FillCostResponseContainer>(&container);
                EXPECT_NE(response, nullptr);
                if (response == nullptr) {
                    return std::unexpected{-1};
                }

                EXPECT_FALSE(response->total_cost.has_value());
                return std::expected<void, int>{};
            }));

    process_container(fill_cost_query, test_limit_order_books, trade_events, mock_ws, 0, 1);
}
