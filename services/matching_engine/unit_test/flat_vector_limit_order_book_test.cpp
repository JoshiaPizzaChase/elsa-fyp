#include "flat_vector_limit_order_book.h"
#include "limit_order_book.h"

using namespace engine;

namespace {
constexpr std::string_view TEST_TICKER{"GME"};
constexpr std::string_view TEST_BROKER{"BROKER_1"};

class StubTradePublisher : public Publisher<Trade> {
  public:
    bool try_publish(Trade&) override {
        return true;
    }
};

struct TradeSnapshot {
    std::string ticker;
    int price{};
    int quantity{};
    std::string taker_id;
    std::string maker_id;
    int taker_order_id{};
    int maker_order_id{};
    bool is_taker_buyer{};

    bool operator==(const TradeSnapshot&) const = default;
};

TradeSnapshot snapshot(const Trade& trade) {
    return TradeSnapshot{trade.ticker,         trade.price,      trade.quantity,
                         trade.taker_id,       trade.maker_id,   trade.taker_order_id,
                         trade.maker_order_id, trade.is_taker_buyer};
}

std::vector<TradeSnapshot> drain_trades(std::queue<Trade>& queue) {
    std::vector<TradeSnapshot> trades;
    while (!queue.empty()) {
        trades.emplace_back(snapshot(queue.front()));
        queue.pop();
    }
    return trades;
}

void expect_same_best_order(const std::optional<std::reference_wrapper<const Order>>& lhs,
                            const std::optional<std::reference_wrapper<const Order>>& rhs) {
    ASSERT_EQ(lhs.has_value(), rhs.has_value());
    if (!lhs.has_value()) {
        return;
    }
    EXPECT_EQ(lhs->get().get_order_id(), rhs->get().get_order_id());
    EXPECT_EQ(lhs->get().get_price(), rhs->get().get_price());
    EXPECT_EQ(lhs->get().get_quantity(), rhs->get().get_quantity());
    EXPECT_EQ(lhs->get().get_side(), rhs->get().get_side());
    EXPECT_EQ(lhs->get().get_trader_id(), rhs->get().get_trader_id());
}

} // namespace

TEST(FlatVectorLimitOrderBookParityTest, MatchingFlowMatchesMapImplementation) {
    std::queue<Trade> map_trades;
    std::queue<Trade> flat_trades;
    LimitOrderBook map_book{TEST_TICKER, map_trades, std::make_unique<StubTradePublisher>()};
    FlatVectorLimitOrderBook flat_book{TEST_TICKER, flat_trades, std::make_unique<StubTradePublisher>()};

    map_book.add_order(1, 100, 5, Side::bid, TEST_BROKER);
    map_book.add_order(2, 99, 5, Side::bid, TEST_BROKER);
    map_book.add_order(3, 98, 5, Side::bid, TEST_BROKER);
    map_book.add_order(4, 99, 20, Side::ask, TEST_BROKER);

    flat_book.add_order(1, 100, 5, Side::bid, TEST_BROKER);
    flat_book.add_order(2, 99, 5, Side::bid, TEST_BROKER);
    flat_book.add_order(3, 98, 5, Side::bid, TEST_BROKER);
    flat_book.add_order(4, 99, 20, Side::ask, TEST_BROKER);

    expect_same_best_order(map_book.get_best_order(Side::bid), flat_book.get_best_order(Side::bid));
    expect_same_best_order(map_book.get_best_order(Side::ask), flat_book.get_best_order(Side::ask));
    EXPECT_EQ(map_book.order_id_exists(1), flat_book.order_id_exists(1));
    EXPECT_EQ(map_book.order_id_exists(2), flat_book.order_id_exists(2));
    EXPECT_EQ(map_book.order_id_exists(3), flat_book.order_id_exists(3));
    EXPECT_EQ(map_book.order_id_exists(4), flat_book.order_id_exists(4));
    EXPECT_EQ(drain_trades(map_trades), drain_trades(flat_trades));
}

TEST(FlatVectorLimitOrderBookParityTest, CancelAndAggregateFlowMatchesMapImplementation) {
    std::queue<Trade> map_trades;
    std::queue<Trade> flat_trades;
    LimitOrderBook map_book{TEST_TICKER, map_trades, std::make_unique<StubTradePublisher>()};
    FlatVectorLimitOrderBook flat_book{TEST_TICKER, flat_trades, std::make_unique<StubTradePublisher>()};

    map_book.add_order(10, 100, 10, Side::ask, TEST_BROKER);
    map_book.add_order(11, 100, 20, Side::ask, TEST_BROKER);
    map_book.add_order(12, 101, 10, Side::ask, TEST_BROKER);
    map_book.cancel_order(11);

    flat_book.add_order(10, 100, 10, Side::ask, TEST_BROKER);
    flat_book.add_order(11, 100, 20, Side::ask, TEST_BROKER);
    flat_book.add_order(12, 101, 10, Side::ask, TEST_BROKER);
    flat_book.cancel_order(11);

    EXPECT_EQ(map_book.get_fill_cost(40, Side::ask), flat_book.get_fill_cost(40, Side::ask));

    const auto map_top = map_book.get_top_order_book_level_aggregate();
    const auto flat_top = flat_book.get_top_order_book_level_aggregate();
    for (int i = 0; i < core::constants::ORDER_BOOK_AGGREGATE_LEVELS; ++i) {
        EXPECT_EQ(map_top.bid_level_aggregates.at(i).price, flat_top.bid_level_aggregates.at(i).price);
        EXPECT_EQ(map_top.bid_level_aggregates.at(i).quantity, flat_top.bid_level_aggregates.at(i).quantity);
        EXPECT_EQ(map_top.ask_level_aggregates.at(i).price, flat_top.ask_level_aggregates.at(i).price);
        EXPECT_EQ(map_top.ask_level_aggregates.at(i).quantity, flat_top.ask_level_aggregates.at(i).quantity);
    }
    EXPECT_EQ(drain_trades(map_trades), drain_trades(flat_trades));
}

TEST(FlatVectorLimitOrderBookParityTest, MarketOrderFlowMatchesMapImplementation) {
    std::queue<Trade> map_trades;
    std::queue<Trade> flat_trades;
    LimitOrderBook map_book{TEST_TICKER, map_trades, std::make_unique<StubTradePublisher>()};
    FlatVectorLimitOrderBook flat_book{TEST_TICKER, flat_trades, std::make_unique<StubTradePublisher>()};

    map_book.add_order(20, 100, 10, Side::ask, TEST_BROKER);
    map_book.add_order(21, 101, 10, Side::ask, TEST_BROKER);
    map_book.add_order(22, MARKET_BID_ORDER_PRICE, 25, Side::bid, TEST_BROKER);

    flat_book.add_order(20, 100, 10, Side::ask, TEST_BROKER);
    flat_book.add_order(21, 101, 10, Side::ask, TEST_BROKER);
    flat_book.add_order(22, MARKET_BID_ORDER_PRICE, 25, Side::bid, TEST_BROKER);

    EXPECT_FALSE(flat_book.get_best_order(Side::bid).has_value());
    EXPECT_FALSE(flat_book.get_best_order(Side::ask).has_value());
    EXPECT_EQ(drain_trades(map_trades), drain_trades(flat_trades));
}
