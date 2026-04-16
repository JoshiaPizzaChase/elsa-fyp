#include <benchmark/benchmark.h>

#include "order_manager.h"

#include <cassert>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
constexpr std::string_view BENCH_SYMBOL = "AAPL";
constexpr std::string_view BENCH_SENDER = "CLIENT";
constexpr std::string_view BENCH_TARGET = "OM";

class BenchmarkOutboundClient final : public transport::OutboundClient {
  public:
    std::expected<void, int> start() override {
        return {};
    }

    std::expected<int, int> connect(std::string_view, std::string_view) override {
        return 1;
    }

    std::optional<std::string> dequeue_message(int) override {
        return std::nullopt;
    }

    std::optional<std::string> wait_and_dequeue_message(int) override {
        return std::nullopt;
    }

    std::expected<void, int> send(int, const std::string&, transport::MessageFormat) override {
        ++send_count;
        return {};
    }

    std::uint64_t send_count{0};
};

core::Container make_new_order(int cl_ord_id, int qty = 1) {
    return core::NewOrderSingleContainer{
        .sender_comp_id = std::string{BENCH_SENDER},
        .target_comp_id = std::string{BENCH_TARGET},
        .order_id = std::nullopt,
        .cl_ord_id = cl_ord_id,
        .symbol = std::string{BENCH_SYMBOL},
        .side = core::Side::ask,
        .order_qty = qty,
        .ord_type = core::OrderType::limit,
        .price = 100,
        .time_in_force = core::TimeInForce::gtc,
    };
}

core::Container make_cancel_request(int orig_cl_ord_id, int cl_ord_id, std::optional<int> order_id) {
    return core::CancelOrderRequestContainer{
        .sender_comp_id = std::string{BENCH_SENDER},
        .target_comp_id = std::string{BENCH_TARGET},
        .order_id = order_id,
        .orig_cl_ord_id = orig_cl_ord_id,
        .cl_ord_id = cl_ord_id,
        .symbol = std::string{BENCH_SYMBOL},
        .side = core::Side::ask,
        .order_qty = 1,
    };
}

static void BM_OrderManager_PreprocessNewOrderBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();

        om::OrderManager::OrderIdMapContainer order_id_map;
        om::OrderManager::OrderInfoMapContainer order_info_map;
        om::OrderManager::UsernameToUserIdMapContainer username_user_id_map;
        username_user_id_map.emplace(std::string{BENCH_SENDER}, 1);

        BenchmarkOutboundClient order_request_client;
        std::vector<core::Container> incoming_orders;
        incoming_orders.reserve(order_count);
        for (int i = 0; i < order_count; ++i) {
            incoming_orders.emplace_back(make_new_order(i + 1, 1));
        }

        state.ResumeTiming();

        for (auto& container : incoming_orders) {
            const auto result = om::preprocess_container(container, order_id_map, order_info_map,
                                                         username_user_id_map, 0,
                                                         order_request_client, 1);
            assert(result.has_value() && "Benchmark setup must produce valid preprocess inputs");
        }

        benchmark::DoNotOptimize(order_info_map.size());
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

static void BM_OrderManager_PreprocessCancelOrderBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));
    constexpr int user_id = 1;

    for (auto _ : state) {
        state.PauseTiming();

        om::OrderManager::OrderIdMapContainer order_id_map;
        om::OrderManager::OrderInfoMapContainer order_info_map;
        om::OrderManager::UsernameToUserIdMapContainer username_user_id_map;
        username_user_id_map.emplace(std::string{BENCH_SENDER}, user_id);

        BenchmarkOutboundClient order_request_client;
        std::vector<core::Container> cancel_requests;
        cancel_requests.reserve(order_count);

        for (int i = 0; i < order_count; ++i) {
            const int orig_cl_ord_id = i + 1;
            const int internal_order_id = i + 10'000;
            const int transformed_orig_cl_ord_id =
                orig_cl_ord_id * core::constants::max_user_count + user_id;
            order_id_map.insert(om::OrderManager::OrderIdPair(internal_order_id, transformed_orig_cl_ord_id));
            cancel_requests.emplace_back(make_cancel_request(orig_cl_ord_id, orig_cl_ord_id + 100'000,
                                                             std::nullopt));
        }

        state.ResumeTiming();

        for (auto& container : cancel_requests) {
            const auto result = om::preprocess_container(container, order_id_map, order_info_map,
                                                         username_user_id_map, 0,
                                                         order_request_client, 1);
            assert(result.has_value() && "Benchmark setup must produce valid preprocess inputs");
        }

        benchmark::DoNotOptimize(order_id_map.size());
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

static void BM_OrderManager_ValidateNewOrderBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));
    const std::unordered_set<std::string> active_symbols{std::string{BENCH_SYMBOL}};

    for (auto _ : state) {
        state.PauseTiming();

        om::BalanceChecker balance_checker;
        balance_checker.update_balance(std::string{BENCH_SENDER}, std::string{BENCH_SYMBOL},
                                       order_count);

        std::vector<core::Container> new_orders;
        new_orders.reserve(order_count);
        for (int i = 0; i < order_count; ++i) {
            auto container = make_new_order(i + 1);
            std::get<core::NewOrderSingleContainer>(container).order_id = i + 1;
            new_orders.emplace_back(std::move(container));
        }

        state.ResumeTiming();

        for (const auto& container : new_orders) {
            const auto result = om::validate_container(container, active_symbols, balance_checker);
            assert(result == "ok" && "Benchmark setup must produce valid validation inputs");
        }

        benchmark::DoNotOptimize(balance_checker);
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

static void BM_OrderManager_ValidateCancelOrderBurstLatency(benchmark::State& state) {
    const int order_count = static_cast<int>(state.range(0));
    const std::unordered_set<std::string> active_symbols{std::string{BENCH_SYMBOL}};

    for (auto _ : state) {
        state.PauseTiming();

        om::BalanceChecker balance_checker;
        std::vector<core::Container> cancel_requests;
        cancel_requests.reserve(order_count);
        for (int i = 0; i < order_count; ++i) {
            cancel_requests.emplace_back(make_cancel_request(i + 1, i + 100'000, i + 10'000));
        }

        state.ResumeTiming();

        for (const auto& container : cancel_requests) {
            const auto result = om::validate_container(container, active_symbols, balance_checker);
            assert(result == "ok" && "Benchmark setup must produce valid validation inputs");
        }

        benchmark::DoNotOptimize(balance_checker);
    }

    state.SetItemsProcessed(state.iterations() * order_count);
}

} // namespace

BENCHMARK(BM_OrderManager_PreprocessNewOrderBurstLatency)->Arg(100)->Arg(1000)->Arg(10000)->Unit(
    benchmark::kMillisecond);

BENCHMARK(BM_OrderManager_PreprocessCancelOrderBurstLatency)->Arg(100)->Arg(1000)->Arg(10000)->Unit(
    benchmark::kMillisecond);

BENCHMARK(BM_OrderManager_ValidateNewOrderBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_OrderManager_ValidateCancelOrderBurstLatency)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
