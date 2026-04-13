#include <benchmark/benchmark.h>

#include "matching_engine.h"
#include "transport/messaging.h"

#include <expected>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
constexpr std::string_view BENCH_SYMBOL = "AAPL";
constexpr int BASE_ASK_PRICE = 100;
constexpr int CROSSING_BID_PRICE = 200;

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
