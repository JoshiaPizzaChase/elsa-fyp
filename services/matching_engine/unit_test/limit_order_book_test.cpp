#include "limit_order_book.h"
#include <gtest/gtest.h>

using namespace engine;

constexpr std::string_view TEST_TICKER{"GME"};
constexpr std::string_view TEST_BROKER{"BROKER_1"};

class FakeTradePublisher : public Publisher<Trade> {
  public:
    bool try_publish(Trade& trade) override {
        return true;
    }
};

class GettersTest : public testing::Test {
  protected:
    std::queue<Trade> trade_events{};
    LimitOrderBook limit_order_book{TEST_TICKER, trade_events,
                                    std::make_unique<FakeTradePublisher>()};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
    }
};
using GettersDeathTest = GettersTest;

TEST_F(GettersDeathTest, GetInvalidOrderID) {
    EXPECT_DEATH(std::ignore = limit_order_book.get_order_by_id(-1), "");
    EXPECT_DEATH(std::ignore = limit_order_book.get_order_by_id(10), "");
}

TEST_F(GettersTest, GetBestOrder) {
    // Existing best bid order
    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    // Non-existent best ask order
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask).has_value());
}

TEST_F(GettersTest, GetOrderByID) {
    auto order = limit_order_book.get_order_by_id(0);

    EXPECT_EQ(order.get_order_id(), 0);
    EXPECT_EQ(order.get_price(), 100);
    EXPECT_EQ(order.get_quantity(), 10);
    EXPECT_EQ(order.get_side(), Side::bid);
}

class MatchingLogicTest : public testing::Test {
  protected:
    std::queue<Trade> trade_events{};
    LimitOrderBook limit_order_book{TEST_TICKER, trade_events,
                                    std::make_unique<FakeTradePublisher>()};
};
using MatchingLogicDeathTest = MatchingLogicTest;

TEST_F(MatchingLogicDeathTest, AddInvalidOrders) {
    // Inserting duplicate order_id
    EXPECT_DEATH(
        {
            limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
            limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
        },
        "");

    // Negative order id
    EXPECT_DEATH(limit_order_book.add_order(-1, 0, 10, Side::ask, TEST_BROKER), "");

    // Non-positive price
    EXPECT_DEATH(limit_order_book.add_order(0, 0, 10, Side::ask, TEST_BROKER), "");
    EXPECT_DEATH(limit_order_book.add_order(0, -10, 10, Side::ask, TEST_BROKER), "");

    // Non-positive quantity
    EXPECT_DEATH(limit_order_book.add_order(0, 10, 0, Side::ask, TEST_BROKER), "");
    EXPECT_DEATH(limit_order_book.add_order(0, 10, -10, Side::ask, TEST_BROKER), "");

    // Empty broker id
    EXPECT_DEATH(limit_order_book.add_order(0, 10, 10, Side::bid, ""), "");
}

TEST_F(MatchingLogicTest, AddBidOrderNoMatch) {
    limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, AddAskOrderNoMatch) {
    limit_order_book.add_order(0, 100, 10, Side::ask, TEST_BROKER);

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, MatchExistingBidOrderCompletely) {
    limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
    limit_order_book.add_order(1, 100, 10, Side::ask, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 10);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_FALSE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, MatchExistingAskOrderCompletely) {
    limit_order_book.add_order(0, 100, 10, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 100, 10, Side::bid, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 10);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_TRUE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, PartialFillStandingBidOrder) {
    limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
    limit_order_book.add_order(1, 100, 5, Side::ask, TEST_BROKER);

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask).has_value());
    EXPECT_FALSE(limit_order_book.order_id_exists(1));

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 5);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_FALSE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, PartialFillStandingAskOrder) {
    limit_order_book.add_order(0, 100, 10, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 100, 5, Side::bid, TEST_BROKER);

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid).has_value());
    EXPECT_FALSE(limit_order_book.order_id_exists(1));

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 5);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_TRUE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, CompletelyFillStandingBidOrder) {
    limit_order_book.add_order(0, 100, 5, Side::bid, TEST_BROKER);
    limit_order_book.add_order(1, 100, 10, Side::ask, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.order_id_exists(0));

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 1);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 5);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_FALSE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, CompletelyFillStandingAskOrder) {
    limit_order_book.add_order(0, 100, 5, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 100, 10, Side::bid, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));
    EXPECT_FALSE(limit_order_book.order_id_exists(0));

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 1);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    ASSERT_TRUE(!trade_events.empty());
    const auto& trade = trade_events.front();
    EXPECT_EQ(trade.ticker, TEST_TICKER);
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 5);
    EXPECT_EQ(trade.taker_id, TEST_BROKER);
    EXPECT_EQ(trade.maker_id, TEST_BROKER);
    EXPECT_EQ(trade.taker_order_id, 1);
    EXPECT_EQ(trade.maker_order_id, 0);
    EXPECT_TRUE(trade.is_taker_buyer);
}

TEST_F(MatchingLogicTest, MatchMultipleStandingBidOrders) {
    limit_order_book.add_order(0, 100, 5, Side::bid, TEST_BROKER);
    limit_order_book.add_order(1, 99, 5, Side::bid, TEST_BROKER);
    limit_order_book.add_order(2, 98, 5, Side::bid, TEST_BROKER);
    limit_order_book.add_order(3, 99, 20, Side::ask, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 2);
                          EXPECT_EQ(order.get_price(), 98);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 3);
                          EXPECT_EQ(order.get_price(), 99);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    ASSERT_EQ(trade_events.size(), 2);

    const auto& trade_1 = trade_events.front();
    EXPECT_EQ(trade_1.ticker, TEST_TICKER);
    EXPECT_EQ(trade_1.price, 100);
    EXPECT_EQ(trade_1.quantity, 5);
    EXPECT_EQ(trade_1.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.taker_order_id, 3);
    EXPECT_EQ(trade_1.maker_order_id, 0);
    EXPECT_FALSE(trade_1.is_taker_buyer);
    trade_events.pop();

    const auto& trade_2 = trade_events.front();
    EXPECT_EQ(trade_2.ticker, TEST_TICKER);
    EXPECT_EQ(trade_2.price, 99);
    EXPECT_EQ(trade_2.quantity, 5);
    EXPECT_EQ(trade_2.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.taker_order_id, 3);
    EXPECT_EQ(trade_2.maker_order_id, 1);
    EXPECT_FALSE(trade_2.is_taker_buyer);
}

TEST_F(MatchingLogicTest, MatchMultipleStandingAskOrders) {
    limit_order_book.add_order(0, 100, 5, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 101, 5, Side::ask, TEST_BROKER);
    limit_order_book.add_order(2, 102, 5, Side::ask, TEST_BROKER);
    limit_order_book.add_order(3, 101, 20, Side::bid, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 2);
                          EXPECT_EQ(order.get_price(), 102);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 3);
                          EXPECT_EQ(order.get_price(), 101);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          EXPECT_EQ(order.get_trader_id(), TEST_BROKER);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    ASSERT_EQ(trade_events.size(), 2);

    const auto& trade_1 = trade_events.front();
    EXPECT_EQ(trade_1.ticker, TEST_TICKER);
    EXPECT_EQ(trade_1.price, 100);
    EXPECT_EQ(trade_1.quantity, 5);
    EXPECT_EQ(trade_1.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.taker_order_id, 3);
    EXPECT_EQ(trade_1.maker_order_id, 0);
    EXPECT_TRUE(trade_1.is_taker_buyer);
    trade_events.pop();

    const auto& trade_2 = trade_events.front();
    EXPECT_EQ(trade_2.ticker, TEST_TICKER);
    EXPECT_EQ(trade_2.price, 101);
    EXPECT_EQ(trade_2.quantity, 5);
    EXPECT_EQ(trade_2.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.taker_order_id, 3);
    EXPECT_EQ(trade_2.maker_order_id, 1);
    EXPECT_TRUE(trade_2.is_taker_buyer);
}

TEST_F(MatchingLogicTest, PartiallyFillMarketBid) {
    limit_order_book.add_order(0, 100, 10, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 101, 10, Side::ask, TEST_BROKER);
    limit_order_book.add_order(2, MARKET_BID_ORDER_PRICE, 25, Side::bid,
                               TEST_BROKER); // Market order

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));

    ASSERT_EQ(trade_events.size(), 2);

    const auto& trade_1 = trade_events.front();
    EXPECT_EQ(trade_1.ticker, TEST_TICKER);
    EXPECT_EQ(trade_1.price, 100);
    EXPECT_EQ(trade_1.quantity, 10);
    EXPECT_EQ(trade_1.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.taker_order_id, 2);
    EXPECT_EQ(trade_1.maker_order_id, 0);
    EXPECT_TRUE(trade_1.is_taker_buyer);
    trade_events.pop();

    const auto& trade_2 = trade_events.front();
    EXPECT_EQ(trade_2.ticker, TEST_TICKER);
    EXPECT_EQ(trade_2.price, 101);
    EXPECT_EQ(trade_2.quantity, 10);
    EXPECT_EQ(trade_2.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.taker_order_id, 2);
    EXPECT_EQ(trade_2.maker_order_id, 1);
    EXPECT_TRUE(trade_2.is_taker_buyer);
}

TEST_F(MatchingLogicTest, PartiallyFillMarketAsk) {
    limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
    limit_order_book.add_order(1, 101, 10, Side::bid, TEST_BROKER);
    limit_order_book.add_order(2, MARKET_ASK_ORDER_PRICE, 25, Side::ask,
                               TEST_BROKER); // Market order

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));

    ASSERT_EQ(trade_events.size(), 2);

    const auto& trade_1 = trade_events.front();
    EXPECT_EQ(trade_1.ticker, TEST_TICKER);
    EXPECT_EQ(trade_1.price, 101);
    EXPECT_EQ(trade_1.quantity, 10);
    EXPECT_EQ(trade_1.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.taker_order_id, 2);
    EXPECT_EQ(trade_1.maker_order_id, 1);
    EXPECT_FALSE(trade_1.is_taker_buyer);
    trade_events.pop();

    const auto& trade_2 = trade_events.front();
    EXPECT_EQ(trade_2.ticker, TEST_TICKER);
    EXPECT_EQ(trade_2.price, 100);
    EXPECT_EQ(trade_2.quantity, 10);
    EXPECT_EQ(trade_2.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.taker_order_id, 2);
    EXPECT_EQ(trade_2.maker_order_id, 0);
    EXPECT_FALSE(trade_2.is_taker_buyer);
}

TEST_F(MatchingLogicTest, AddMarketOrderToEmptyBook) {
    limit_order_book.add_order(0, MARKET_BID_ORDER_PRICE, 10, Side::bid, TEST_BROKER);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
}

class CancelOrderTest : public testing::Test {
  protected:
    std::queue<Trade> trade_events{};
    LimitOrderBook limit_order_book{TEST_TICKER, trade_events,
                                    std::make_unique<FakeTradePublisher>()};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
    }
};
using CancelOrderDeathTest = CancelOrderTest;

TEST_F(CancelOrderDeathTest, CancelInvalidOrder) {
    EXPECT_DEATH(limit_order_book.cancel_order(-1), "");
    EXPECT_DEATH(limit_order_book.cancel_order(10), "");
}

TEST_F(CancelOrderTest, CancelExistingOrder) {
    limit_order_book.cancel_order(0);

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
}

class LevelAggregateTest : public testing::Test {
  protected:
    std::queue<Trade> trade_events{};
    LimitOrderBook limit_order_book{TEST_TICKER, trade_events,
                                    std::make_unique<FakeTradePublisher>()};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid, TEST_BROKER);
        limit_order_book.add_order(1, 100, 20, Side::bid, TEST_BROKER);
        limit_order_book.add_order(2, 101, 10, Side::bid, TEST_BROKER);
    }

    void add_ask_order() {
        limit_order_book.add_order(3, 200, 10, Side::ask, TEST_BROKER);
        limit_order_book.add_order(4, 200, 20, Side::ask, TEST_BROKER);
        limit_order_book.add_order(5, 201, 10, Side::ask, TEST_BROKER);
    }
};
using LevelAggregateDeathTest = LevelAggregateTest;

TEST_F(LevelAggregateDeathTest, GetInvalidLevelAggregate) {
    // Empty side
    EXPECT_DEATH(std::ignore = limit_order_book.get_level_aggregate(Side::ask, 0), "");

    // Invalid level
    EXPECT_DEATH(std::ignore = limit_order_book.get_level_aggregate(Side::bid, -1), "");
    EXPECT_DEATH(std::ignore = limit_order_book.get_level_aggregate(Side::bid, 10), "");
}

TEST_F(LevelAggregateTest, GetLevelAggregate) {
    // Get bid aggregates
    const auto level_zero_bid = limit_order_book.get_level_aggregate(Side::bid, 0);
    EXPECT_EQ(level_zero_bid.price, 101);
    EXPECT_EQ(level_zero_bid.quantity, 10);

    const auto level_one_bid = limit_order_book.get_level_aggregate(Side::bid, 1);
    EXPECT_EQ(level_one_bid.price, 100);
    EXPECT_EQ(level_one_bid.quantity, 30);

    // Get ask aggregates
    add_ask_order();
    const auto level_zero_ask = limit_order_book.get_level_aggregate(Side::ask, 0);
    EXPECT_EQ(level_zero_ask.price, 200);
    EXPECT_EQ(level_zero_ask.quantity, 30);

    const auto level_one_ask = limit_order_book.get_level_aggregate(Side::ask, 1);
    EXPECT_EQ(level_one_ask.price, 201);
    EXPECT_EQ(level_one_ask.quantity, 10);
}

TEST_F(LevelAggregateTest, GetTopOrderBookAggregate) {
    // Empty Order Book
    const auto empty_limit_order_book =
        LimitOrderBook{TEST_TICKER, trade_events, std::make_unique<FakeTradePublisher>()};
    const auto empty_lob_top_aggregate =
        empty_limit_order_book.get_top_order_book_level_aggregate();

    for (int i = 0; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; i++) {
        EXPECT_EQ(empty_lob_top_aggregate.bid_level_aggregates.at(i).price, 0);
        EXPECT_EQ(empty_lob_top_aggregate.bid_level_aggregates.at(i).price, 0);

        EXPECT_EQ(empty_lob_top_aggregate.ask_level_aggregates.at(i).quantity, 0);
        EXPECT_EQ(empty_lob_top_aggregate.ask_level_aggregates.at(i).quantity, 0);
    }

    // Filled Order Book
    auto top_aggregate = limit_order_book.get_top_order_book_level_aggregate();
    // Bid aggregates
    EXPECT_EQ(top_aggregate.bid_level_aggregates.at(0).price, 101);
    EXPECT_EQ(top_aggregate.bid_level_aggregates.at(0).quantity, 10);

    EXPECT_EQ(top_aggregate.bid_level_aggregates.at(1).price, 100);
    EXPECT_EQ(top_aggregate.bid_level_aggregates.at(1).quantity, 30);

    for (int i = 2; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; i++) {
        EXPECT_EQ(top_aggregate.bid_level_aggregates.at(i).price, 0);
        EXPECT_EQ(top_aggregate.bid_level_aggregates.at(i).quantity, 0);
    }

    // Ask aggregates
    add_ask_order();
    top_aggregate = limit_order_book.get_top_order_book_level_aggregate();
    EXPECT_EQ(top_aggregate.ask_level_aggregates.at(0).price, 200);
    EXPECT_EQ(top_aggregate.ask_level_aggregates.at(0).quantity, 30);

    EXPECT_EQ(top_aggregate.ask_level_aggregates.at(1).price, 201);
    EXPECT_EQ(top_aggregate.ask_level_aggregates.at(1).quantity, 10);

    for (int i = 2; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; i++) {
        EXPECT_EQ(top_aggregate.ask_level_aggregates.at(i).price, 0);
        EXPECT_EQ(top_aggregate.ask_level_aggregates.at(i).quantity, 0);
    }
}

class FillCostQueryTest : public testing::Test {
  protected:
    std::queue<Trade> trade_events{};
    LimitOrderBook limit_order_book{TEST_TICKER, trade_events,
                                    std::make_unique<FakeTradePublisher>()};
};
using FillCostQueryDeathTest = FillCostQueryTest;

TEST_F(FillCostQueryDeathTest, GetWithInvalidQuantity) {
    EXPECT_DEATH(std::ignore = limit_order_book.get_fill_cost(-1, Side::bid), "");
    EXPECT_DEATH(std::ignore = limit_order_book.get_fill_cost(0, Side::bid), "");
}

TEST_F(FillCostQueryTest, GetFillCost) {
    // Empty Book
    EXPECT_FALSE(limit_order_book.get_fill_cost(10, Side::bid));

    limit_order_book.add_order(0, 100, 10, Side::ask, TEST_BROKER);
    limit_order_book.add_order(1, 100, 20, Side::ask, TEST_BROKER);
    limit_order_book.add_order(2, 101, 10, Side::ask, TEST_BROKER);

    // Sufficient Liquidity
    std::ignore = limit_order_book.get_fill_cost(40, Side::ask)
                      .transform([](int cost) -> int {
                          EXPECT_EQ(cost, 4010);
                          return cost;
                      })
                      .or_else([]() -> std::optional<int> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    // Insufficient Liquidity
    std::ignore = limit_order_book.get_fill_cost(100, Side::ask)
                      .transform([](int cost) -> int {
                          EXPECT_EQ(cost, 4010);
                          return cost;
                      })
                      .or_else([]() -> std::optional<int> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST(CreateTradeDeathTest, CreateInvalidTrade) {
    // Negative taker order id
    EXPECT_DEATH(std::ignore =
                     create_trade(-1, 0, TEST_BROKER, TEST_BROKER, TEST_TICKER, 100, 10, Side::bid),
                 "");

    // Negative maker order id
    EXPECT_DEATH(std::ignore =
                     create_trade(0, -1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 100, 10, Side::bid),
                 "");

    // Colliding taker maker order id
    EXPECT_DEATH(std::ignore =
                     create_trade(0, 0, TEST_BROKER, TEST_BROKER, TEST_TICKER, 100, 10, Side::bid),
                 "");

    // Empty taker id
    EXPECT_DEATH(std::ignore = create_trade(0, 1, "", TEST_BROKER, TEST_TICKER, 100, 10, Side::bid),
                 "");

    // Empty maker id
    EXPECT_DEATH(std::ignore = create_trade(0, 1, TEST_BROKER, "", TEST_TICKER, 100, 10, Side::bid),
                 "");

    // Empty ticker
    EXPECT_DEATH(std::ignore = create_trade(0, 1, TEST_BROKER, TEST_BROKER, "", 100, 10, Side::bid),
                 "");

    // Non-positive price
    EXPECT_DEATH(std::ignore =
                     create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 0, 10, Side::bid),
                 "");
    EXPECT_DEATH(std::ignore =
                     create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, -10, 10, Side::bid),
                 "");

    // Non-positive quantity
    EXPECT_DEATH(std::ignore =
                     create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 10, 0, Side::bid),
                 "");
    EXPECT_DEATH(std::ignore =
                     create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 10, -10, Side::bid),
                 "");
}

TEST(CreateTradeTest, CreateValidTrade) {
    const auto now_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    const auto trade_1 =
        create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 100, 10, Side::bid);

    EXPECT_EQ(trade_1.taker_order_id, 0);
    EXPECT_EQ(trade_1.maker_order_id, 1);
    EXPECT_EQ(trade_1.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_1.ticker, TEST_TICKER);
    EXPECT_EQ(trade_1.price, 100);
    EXPECT_EQ(trade_1.quantity, 10);
    EXPECT_TRUE(trade_1.is_taker_buyer);
    EXPECT_EQ(std::strlen(trade_1.trade_id),
              core::constants::UUID_LENGTH -
                  1); // Ensure trade id is uuid, -1 because of null terminator
    EXPECT_GE(trade_1.create_timestamp, now_ts);

    const auto trade_2 =
        create_trade(0, 1, TEST_BROKER, TEST_BROKER, TEST_TICKER, 100, 10, Side::ask);
    EXPECT_EQ(trade_2.taker_order_id, 0);
    EXPECT_EQ(trade_2.maker_order_id, 1);
    EXPECT_EQ(trade_2.taker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.maker_id, TEST_BROKER);
    EXPECT_EQ(trade_2.ticker, TEST_TICKER);
    EXPECT_EQ(trade_2.price, 100);
    EXPECT_EQ(trade_2.quantity, 10);
    EXPECT_FALSE(trade_2.is_taker_buyer);
    EXPECT_EQ(std::strlen(trade_2.trade_id),
              core::constants::UUID_LENGTH -
                  1); // Ensure trade id is uuid, -1 because of null terminator
    EXPECT_GE(trade_2.create_timestamp, now_ts);
}