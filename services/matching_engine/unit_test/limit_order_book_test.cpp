#include "limit_order_book.h"
#include <gtest/gtest.h>

using namespace engine;

constexpr std::string_view TEST_TICKER{"GME"};

class GettersTest : public testing::Test {
  protected:
    LimitOrderBook limit_order_book{TEST_TICKER};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid);
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
    LimitOrderBook limit_order_book{TEST_TICKER};
};
using MatchingLogicDeathTest = MatchingLogicTest;

TEST_F(MatchingLogicDeathTest, AddInvalidOrders) {
    // Inserting duplicate order_id
    EXPECT_DEATH(
        {
            limit_order_book.add_order(0, 100, 10, Side::bid);
            limit_order_book.add_order(0, 100, 10, Side::bid);
        },
        "");

    // Non-positive price
    EXPECT_DEATH(limit_order_book.add_order(0, 0, 10, Side::ask), "");
    EXPECT_DEATH(limit_order_book.add_order(0, -10, 10, Side::ask), "");

    // Non-positive quantity
    EXPECT_DEATH(limit_order_book.add_order(0, 10, 0, Side::ask), "");
    EXPECT_DEATH(limit_order_book.add_order(0, 10, -10, Side::ask), "");
}

TEST_F(MatchingLogicTest, AddBidOrderNoMatch) {
    limit_order_book.add_order(0, 100, 10, Side::bid);

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, AddAskOrderNoMatch) {
    limit_order_book.add_order(0, 100, 10, Side::ask);

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 10);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, MatchExistingBidOrderCompletely) {
    limit_order_book.add_order(0, 100, 10, Side::bid);
    limit_order_book.add_order(1, 100, 10, Side::ask);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
}

TEST_F(MatchingLogicTest, MatchExistingAskOrderCompletely) {
    limit_order_book.add_order(0, 100, 10, Side::ask);
    limit_order_book.add_order(1, 100, 10, Side::bid);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
}

TEST_F(MatchingLogicTest, PartialFillStandingBidOrder) {
    limit_order_book.add_order(0, 100, 10, Side::bid);
    limit_order_book.add_order(1, 100, 5, Side::ask);

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask).has_value());
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
}

TEST_F(MatchingLogicTest, PartialFillStandingAskOrder) {
    limit_order_book.add_order(0, 100, 10, Side::ask);
    limit_order_book.add_order(1, 100, 5, Side::bid);

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 0);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid).has_value());
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
}

TEST_F(MatchingLogicTest, CompletelyFillStandingBidOrder) {
    limit_order_book.add_order(0, 100, 5, Side::bid);
    limit_order_book.add_order(1, 100, 10, Side::ask);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.order_id_exists(0));

    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 1);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, CompletelyFillStandingAskOrder) {
    limit_order_book.add_order(0, 100, 5, Side::ask);
    limit_order_book.add_order(1, 100, 10, Side::bid);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));
    EXPECT_FALSE(limit_order_book.order_id_exists(0));

    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 1);
                          EXPECT_EQ(order.get_price(), 100);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, MatchMultipleStandingBidOrders) {
    limit_order_book.add_order(0, 100, 5, Side::bid);
    limit_order_book.add_order(1, 99, 5, Side::bid);
    limit_order_book.add_order(2, 98, 5, Side::bid);
    limit_order_book.add_order(3, 99, 20, Side::ask);

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
    std::ignore = limit_order_book.get_best_order(Side::bid)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 2);
                          EXPECT_EQ(order.get_price(), 98);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::bid);
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
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, MatchMultipleStandingAskOrders) {
    limit_order_book.add_order(0, 100, 5, Side::ask);
    limit_order_book.add_order(1, 101, 5, Side::ask);
    limit_order_book.add_order(2, 102, 5, Side::ask);
    limit_order_book.add_order(3, 101, 20, Side::bid);

    EXPECT_FALSE(limit_order_book.order_id_exists(0));
    EXPECT_FALSE(limit_order_book.order_id_exists(1));
    std::ignore = limit_order_book.get_best_order(Side::ask)
                      .transform([](Order order) -> Order {
                          EXPECT_EQ(order.get_order_id(), 2);
                          EXPECT_EQ(order.get_price(), 102);
                          EXPECT_EQ(order.get_quantity(), 5);
                          EXPECT_EQ(order.get_side(), Side::ask);
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
                          return order;
                      })
                      .or_else([]() -> std::optional<Order> {
                          ADD_FAILURE();
                          return std::nullopt;
                      });
}

TEST_F(MatchingLogicTest, PartiallyFillMarketBid) {

    limit_order_book.add_order(0, 100, 10, Side::ask);
    limit_order_book.add_order(1, 101, 10, Side::ask);
    limit_order_book.add_order(2, MARKET_BID_ORDER_PRICE, 25,
                               Side::bid); // Market order

    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
}

TEST_F(MatchingLogicTest, PartiallyFillMarketAsk) {

    limit_order_book.add_order(0, 100, 10, Side::bid);
    limit_order_book.add_order(1, 101, 10, Side::bid);
    limit_order_book.add_order(2, MARKET_ASK_ORDER_PRICE, 25,
                               Side::ask); // Market order

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
    EXPECT_FALSE(limit_order_book.get_best_order(Side::ask));
}

TEST_F(MatchingLogicTest, AddMarketOrderToEmptyBook) {
    limit_order_book.add_order(0, MARKET_BID_ORDER_PRICE, 10, Side::bid);

    EXPECT_FALSE(limit_order_book.get_best_order(Side::bid));
}

class CancelOrderTest : public testing::Test {
  protected:
    LimitOrderBook limit_order_book{TEST_TICKER};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid);
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
    LimitOrderBook limit_order_book{TEST_TICKER};

    void SetUp() override {
        limit_order_book.add_order(0, 100, 10, Side::bid);
        limit_order_book.add_order(1, 100, 20, Side::bid);
        limit_order_book.add_order(2, 101, 10, Side::bid);
    }

    void add_ask_order() {
        limit_order_book.add_order(3, 200, 10, Side::ask);
        limit_order_book.add_order(4, 200, 20, Side::ask);
        limit_order_book.add_order(5, 201, 10, Side::ask);
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
    const auto empty_limit_order_book = LimitOrderBook{TEST_TICKER};
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
    LimitOrderBook limit_order_book{TEST_TICKER};
};
using FillCostQueryDeathTest = FillCostQueryTest;

TEST_F(FillCostQueryDeathTest, GetWithInvalidQuantity) {
    EXPECT_DEATH(std::ignore = limit_order_book.get_fill_cost(-1, Side::bid), "");
    EXPECT_DEATH(std::ignore = limit_order_book.get_fill_cost(0, Side::bid), "");
}

TEST_F(FillCostQueryTest, GetFillCost) {
    // Empty Book
    EXPECT_FALSE(limit_order_book.get_fill_cost(10, Side::bid));

    limit_order_book.add_order(0, 100, 10, Side::ask);
    limit_order_book.add_order(1, 100, 20, Side::ask);
    limit_order_book.add_order(2, 101, 10, Side::ask);

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

// TODO: Unit test for create_trade after finalizing