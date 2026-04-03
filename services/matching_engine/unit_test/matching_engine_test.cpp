#include "matching_engine.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace engine;
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
    ON_CALL(*mock_ws, start()).WillByDefault(Return(std::unexpected{-1}));

    EXPECT_DEATH(test_me.init(), "");
}
