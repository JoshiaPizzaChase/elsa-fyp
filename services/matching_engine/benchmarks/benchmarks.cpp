#include <benchmark/benchmark.h>

#include "flat_vector_limit_order_book.h"
#include "matching_engine.h"
#include "transport/messaging.h"
#include "vector_limit_order_book.h"

#include <algorithm>
#include <expected>
#include <random>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
constexpr std::string_view BENCH_SYMBOL = "AAPL";
constexpr int BASE_ASK_PRICE = 100;
constexpr int CROSSING_BID_PRICE = 200;
constexpr int MID_PRICE = 10'000;
constexpr int PRICE_LEVELS = 30;
constexpr int MAX_PASSIVE_OFFSET = 3;
constexpr int MAX_QTY = 10;
constexpr double ADD_PROBABILITY = 0.70;
constexpr double CANCEL_PROBABILITY = 0.20;

class NoopTradePublisher final : public engine::Publisher<Trade> {
  public:
    bool try_publish(Trade&) override {
        return true;
    }
};

class BenchmarkInboundServer final : public transport::InboundServer {
  public:
    std::expected<void, int> start() override {
        return {};
    }

    [[nodiscard]] std::vector<transport::InboundConnectionInfo> get_connection_info() const override {
        return {};
    }

    std::optional<std::string> dequeue_message(int) override {
        return std::nullopt;
    }

    std::expected<void, int> send(int, const std::string&, transport::MessageFormat) override {
        ++send_count;
        return {};
    }

    std::uint64_t send_count{0};
};

core::NewOrderSingleContainer make_order(int order_id, core::Side side, int price,
                                         std::string_view sender_comp_id) {
    return core::NewOrderSingleContainer{
        .sender_comp_id = std::string{sender_comp_id},
        .target_comp_id = "ME",
        .order_id = order_id,
        .cl_ord_id = order_id,
        .symbol = std::string{BENCH_SYMBOL},
        .side = side,
        .order_qty = 1,
        .ord_type = core::OrderType::limit,
        .price = price,
        .time_in_force = core::TimeInForce::day,
    };
}

core::CancelOrderRequestContainer make_cancel_request(int order_id, int cl_ord_id,
                                                      core::Side side,
                                                      std::string_view sender_comp_id) {
    return core::CancelOrderRequestContainer{
        .sender_comp_id = std::string{sender_comp_id},
        .target_comp_id = "ME",
        .order_id = order_id,
        .orig_cl_ord_id = cl_ord_id,
        .cl_ord_id = cl_ord_id + 1'000'000,
        .symbol = std::string{BENCH_SYMBOL},
        .side = side,
        .order_qty = 1,
    };
}

core::TradeContainer make_trade_container(int order_id) {
    return core::TradeContainer{
        .ticker = std::string{BENCH_SYMBOL},
        .price = BASE_ASK_PRICE,
        .quantity = 1,
        .trade_id = std::to_string(order_id),
        .taker_id = "TAKER",
        .maker_id = "MAKER",
        .taker_order_id = order_id,
        .maker_order_id = order_id - 1,
        .is_taker_buyer = true,
    };
}

static void BM_LimitOrderBook_AddOrderNoMatchBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        std::queue<Trade> trade_events;
        engine::LimitOrderBook book{
            BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()};

        state.ResumeTiming();

        for (int i = 0; i < order_count; ++i) {
            book.add_order(i + 1, BASE_ASK_PRICE + (i % 5), 1, core::Side::ask, "MAKER");
        }

        benchmark::DoNotOptimize(book);
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

static void BM_VectorLimitOrderBook_AddOrderNoMatchBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        std::queue<Trade> trade_events;
        engine::VectorLimitOrderBook book{
            BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()};

        state.ResumeTiming();

        for (int i = 0; i < order_count; ++i) {
            book.add_order(i + 1, BASE_ASK_PRICE + (i % 5), 1, core::Side::ask, "MAKER");
        }

        benchmark::DoNotOptimize(book);
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

template <typename BookT> struct LiveOrderPool {
    std::vector<int> order_ids{};

    void add(int order_id) {
        order_ids.emplace_back(order_id);
    }

    bool remove_existing(BookT& book, std::mt19937& rng) {
        if (order_ids.empty()) {
            return false;
        }

        std::uniform_int_distribution<std::size_t> pick(0, order_ids.size() - 1);
        int tries = 0;
        while (!order_ids.empty() && tries < 8) {
            const std::size_t idx = pick(rng);
            const int order_id = order_ids[idx];
            if (!book.order_id_exists(order_id)) {
                order_ids[idx] = order_ids.back();
                order_ids.pop_back();
                if (!order_ids.empty()) {
                    pick = std::uniform_int_distribution<std::size_t>(0, order_ids.size() - 1);
                }
                ++tries;
                continue;
            }

            book.cancel_order(order_id);
            order_ids[idx] = order_ids.back();
            order_ids.pop_back();
            return true;
        }

        return false;
    }
};

template <typename BookT>
void add_passive_order(BookT& book, int& next_order_id, LiveOrderPool<BookT>& bid_pool,
                       LiveOrderPool<BookT>& ask_pool, std::mt19937& rng) {
    std::bernoulli_distribution choose_bid(0.5);
    std::uniform_int_distribution<int> qty_dist(1, MAX_QTY);
    std::uniform_int_distribution<int> offset_dist(0, MAX_PASSIVE_OFFSET);
    const engine::Side side = choose_bid(rng) ? engine::Side::bid : engine::Side::ask;
    const auto best_bid = book.get_best_order(engine::Side::bid);
    const auto best_ask = book.get_best_order(engine::Side::ask);

    int price = MID_PRICE;
    if (side == engine::Side::bid) {
        if (best_ask.has_value()) {
            price = std::max(1, best_ask->get().get_price() - 1 - offset_dist(rng));
        } else {
            price = MID_PRICE - 1 - offset_dist(rng);
        }
    } else {
        if (best_bid.has_value()) {
            price = best_bid->get().get_price() + 1 + offset_dist(rng);
        } else {
            price = MID_PRICE + 1 + offset_dist(rng);
        }
    }

    const int order_id = next_order_id++;
    book.add_order(order_id, price, qty_dist(rng), side, "MAKER");
    if (side == engine::Side::bid) {
        bid_pool.add(order_id);
    } else {
        ask_pool.add(order_id);
    }
}

template <typename BookT>
void add_aggressive_order(BookT& book, int& next_order_id, std::mt19937& rng) {
    std::bernoulli_distribution choose_bid(0.5);
    std::uniform_int_distribution<int> qty_dist(1, MAX_QTY * 2);
    const engine::Side side = choose_bid(rng) ? engine::Side::bid : engine::Side::ask;
    const int market_price =
        side == engine::Side::bid ? engine::MARKET_BID_ORDER_PRICE : engine::MARKET_ASK_ORDER_PRICE;
    book.add_order(next_order_id++, market_price, qty_dist(rng), side, "TAKER");
}

template <typename BookT>
void prepopulate_book(BookT& book, int& next_order_id, int prepopulate_count,
                      LiveOrderPool<BookT>& bid_pool, LiveOrderPool<BookT>& ask_pool,
                      std::mt19937& rng) {
    std::uniform_int_distribution<int> qty_dist(1, MAX_QTY);
    std::uniform_int_distribution<int> level_dist(0, PRICE_LEVELS - 1);

    for (int i = 0; i < prepopulate_count; ++i) {
        const int level = level_dist(rng);
        const int bid_id = next_order_id++;
        const int bid_price = MID_PRICE - 1 - level;
        book.add_order(bid_id, bid_price, qty_dist(rng), engine::Side::bid, "SEED_BID");
        bid_pool.add(bid_id);

        const int ask_id = next_order_id++;
        const int ask_price = MID_PRICE + 1 + level;
        book.add_order(ask_id, ask_price, qty_dist(rng), engine::Side::ask, "SEED_ASK");
        ask_pool.add(ask_id);
    }
}

template <typename BookT>
void run_realistic_flow_benchmark(benchmark::State& state) {
    const int action_count = static_cast<int>(state.range(0));
    const int prepopulate_count = std::max(250, action_count / 2);

    for (auto _ : state) {
        state.PauseTiming();
        std::queue<Trade> trade_events;
        BookT book{BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()};
        LiveOrderPool<BookT> bid_pool;
        LiveOrderPool<BookT> ask_pool;
        int next_order_id = 1;
        std::mt19937 rng(20260411);
        prepopulate_book(book, next_order_id, prepopulate_count, bid_pool, ask_pool, rng);
        std::discrete_distribution<int> action_dist{
            ADD_PROBABILITY, CANCEL_PROBABILITY, 1.0 - ADD_PROBABILITY - CANCEL_PROBABILITY};
        std::bernoulli_distribution choose_bid(0.5);
        state.ResumeTiming();

        for (int i = 0; i < action_count; ++i) {
            const int action = action_dist(rng); // 0=add, 1=cancel, 2=match
            if (action == 0) {
                add_passive_order(book, next_order_id, bid_pool, ask_pool, rng);
                continue;
            }

            if (action == 1) {
                auto& pool = choose_bid(rng) ? bid_pool : ask_pool;
                if (!pool.remove_existing(book, rng)) {
                    add_passive_order(book, next_order_id, bid_pool, ask_pool, rng);
                }
                continue;
            }

            add_aggressive_order(book, next_order_id, rng);
        }

        benchmark::DoNotOptimize(book);
        benchmark::DoNotOptimize(trade_events.size());
    }

    state.SetItemsProcessed(state.iterations() * action_count);
}

static void BM_LimitOrderBook_RealisticMixedFlow(benchmark::State& state) {
    run_realistic_flow_benchmark<engine::LimitOrderBook>(state);
}

static void BM_VectorLimitOrderBook_RealisticMixedFlow(benchmark::State& state) {
    run_realistic_flow_benchmark<engine::VectorLimitOrderBook>(state);
}

static void BM_FlatVectorLimitOrderBook_RealisticMixedFlow(benchmark::State& state) {
    run_realistic_flow_benchmark<engine::FlatVectorLimitOrderBook>(state);
}

static void BM_MatchingEngine_NewOrderBurstLatency(benchmark::State& state) {
    const int incoming_order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();

        std::queue<Trade> trade_events;
        std::unordered_map<std::string, engine::LimitOrderBook> limit_order_books;
        limit_order_books.emplace(
            BENCH_SYMBOL,
            engine::LimitOrderBook{BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()});
        auto& book = limit_order_books.at(std::string{BENCH_SYMBOL});

        int next_order_id = 1;
        for (int i = 0; i < incoming_order_count; ++i) {
            book.add_order(next_order_id++, BASE_ASK_PRICE + (i % 5), 1, core::Side::ask, "MAKER");
        }

        std::vector<core::Container> incoming_orders;
        incoming_orders.reserve(incoming_order_count);
        for (int i = 0; i < incoming_order_count; ++i) {
            incoming_orders.emplace_back(
                make_order(next_order_id++, core::Side::bid, CROSSING_BID_PRICE, "TAKER"));
        }

        BenchmarkInboundServer inbound_server;

        state.ResumeTiming();

        for (auto& container : incoming_orders) {
            engine::process_container(container, limit_order_books, trade_events, inbound_server, 0, 1);
        }

        benchmark::DoNotOptimize(inbound_server.send_count);
    }

    state.SetItemsProcessed(state.iterations() * incoming_order_count);
}

static void BM_MatchingEngine_NewOrderNoMatchBurstLatency(benchmark::State& state) {
    const int incoming_order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();

        std::queue<Trade> trade_events;
        std::unordered_map<std::string, engine::LimitOrderBook> limit_order_books;
        limit_order_books.emplace(
            BENCH_SYMBOL,
            engine::LimitOrderBook{BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()});
        auto& book = limit_order_books.at(std::string{BENCH_SYMBOL});

        int next_order_id = 1;
        for (int i = 0; i < incoming_order_count; ++i) {
            book.add_order(next_order_id++, CROSSING_BID_PRICE, 1, core::Side::ask, "MAKER");
        }

        std::vector<core::Container> incoming_orders;
        incoming_orders.reserve(incoming_order_count);
        for (int i = 0; i < incoming_order_count; ++i) {
            incoming_orders.emplace_back(
                make_order(next_order_id++, core::Side::bid, BASE_ASK_PRICE, "TAKER"));
        }

        BenchmarkInboundServer inbound_server;

        state.ResumeTiming();

        for (auto& container : incoming_orders) {
            engine::process_container(container, limit_order_books, trade_events, inbound_server, 0, 1);
        }

        benchmark::DoNotOptimize(inbound_server.send_count);
    }

    state.SetItemsProcessed(state.iterations() * incoming_order_count);
}

static void BM_MatchingEngine_CancelOrderBurstLatency(benchmark::State& state) {
    const int cancel_order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();

        std::queue<Trade> trade_events;
        std::unordered_map<std::string, engine::LimitOrderBook> limit_order_books;
        limit_order_books.emplace(
            BENCH_SYMBOL,
            engine::LimitOrderBook{BENCH_SYMBOL, trade_events, std::make_unique<NoopTradePublisher>()});
        auto& book = limit_order_books.at(std::string{BENCH_SYMBOL});

        std::vector<core::Container> cancel_requests;
        cancel_requests.reserve(cancel_order_count);
        for (int i = 0; i < cancel_order_count; ++i) {
            const int order_id = i + 1;
            book.add_order(order_id, BASE_ASK_PRICE + (i % 5), 1, core::Side::ask, "MAKER");
            cancel_requests.emplace_back(
                make_cancel_request(order_id, order_id, core::Side::ask, "CANCELLER"));
        }

        BenchmarkInboundServer inbound_server;

        state.ResumeTiming();

        for (auto& container : cancel_requests) {
            engine::process_container(container, limit_order_books, trade_events, inbound_server, 0, 1);
        }

        benchmark::DoNotOptimize(inbound_server.send_count);
    }

    state.SetItemsProcessed(state.iterations() * cancel_order_count);
}

static void BM_MatchingEngine_TradeSerializationBurstLatency(benchmark::State& state) {
    const int trade_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        for (int i = 0; i < trade_count; ++i) {
            auto payload = transport::serialize_container(make_trade_container(i + 1));
            benchmark::DoNotOptimize(payload);
        }
    }

    state.SetItemsProcessed(state.iterations() * trade_count);
}

} // namespace

BENCHMARK(BM_LimitOrderBook_AddOrderNoMatchBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_VectorLimitOrderBook_AddOrderNoMatchBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_LimitOrderBook_RealisticMixedFlow)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_VectorLimitOrderBook_RealisticMixedFlow)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_FlatVectorLimitOrderBook_RealisticMixedFlow)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_MatchingEngine_NewOrderBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_MatchingEngine_NewOrderNoMatchBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_MatchingEngine_CancelOrderBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_MatchingEngine_TradeSerializationBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
