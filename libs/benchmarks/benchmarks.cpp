#include <benchmark/benchmark.h>

#include "inter_process/mpsc_shared_memory_ring_buffer.h"

#include <atomic>
#include <chrono>
#include <thread>

static void BM_MpscRingBuffer_ProducerConsumerLatency(benchmark::State& state) {
    MpscRingBuffer<std::uint64_t, 1024> ring_buffer;
    ring_buffer.init();

    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> requested_sequence{0};
    std::atomic<std::uint64_t> consumed_sequence{0};

    std::thread producer([&]() {
        std::uint64_t produced{0};
        while (running.load(std::memory_order_acquire)) {
            const auto requested = requested_sequence.load(std::memory_order_acquire);
            if (produced < requested) {
                const std::uint64_t payload = produced + 1;
                if (ring_buffer.try_push(payload)) {
                    produced = payload;
                }
                continue;
            }
            std::this_thread::yield();
        }
    });

    std::thread consumer([&]() {
        while (running.load(std::memory_order_acquire)) {
            if (auto value = ring_buffer.try_pop(); value.has_value()) {
                consumed_sequence.store(value.value(), std::memory_order_release);
                continue;
            }
            std::this_thread::yield();
        }
    });

    std::uint64_t iteration_sequence{0};
    for (auto _ : state) {
        const std::uint64_t current_sequence = ++iteration_sequence;
        const auto start = std::chrono::steady_clock::now();
        requested_sequence.store(current_sequence, std::memory_order_release);

        while (consumed_sequence.load(std::memory_order_acquire) < current_sequence) {
        }

        const auto end = std::chrono::steady_clock::now();
        const auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(static_cast<double>(latency_ns.count()) / 1e9);
        benchmark::DoNotOptimize(consumed_sequence.load(std::memory_order_relaxed));
    }

    running.store(false, std::memory_order_release);
    producer.join();
    consumer.join();
}

BENCHMARK(BM_MpscRingBuffer_ProducerConsumerLatency)
    ->UseManualTime()
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
