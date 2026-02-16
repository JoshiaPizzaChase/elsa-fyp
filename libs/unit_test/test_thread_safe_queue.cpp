#include "core/thread_safe_queue.h" // Assume this includes the class definition
#include <atomic>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

// SECTIONs for readability, like sub-tests
using namespace core;
TEMPLATE_TEST_CASE("BasicEnqueueDequeue", "[ThreadSafeQueue][basic]", int, std::string) {
    ThreadSafeQueue<TestType> queue;
    queue.enqueue(TestType{});
    auto opt = queue.dequeue();
    REQUIRE(opt.has_value());
    REQUIRE(opt.value() == TestType{});
}

TEMPLATE_TEST_CASE("EmptyDequeueReturnsNullopt", "[ThreadSafeQueue][basic]", int, std::string) {
    ThreadSafeQueue<TestType> queue;
    auto opt = queue.dequeue();
    REQUIRE_FALSE(opt.has_value());
}

TEMPLATE_TEST_CASE("MultipleEnqueueDequeueOrderPreserved", "[ThreadSafeQueue][basic]", int,
                   std::string) {
    ThreadSafeQueue<TestType> queue;
    std::vector<TestType> expected_values;
    if constexpr (std::is_same_v<TestType, int>) {
        expected_values = {1, 2, 3};
    } else {
        expected_values = {"one", "two", "three"};
    }
    for (auto& val : expected_values) {
        queue.enqueue(val); // Copy for string, fine
    }
    std::vector<TestType> dequeued;
    while (auto opt = queue.dequeue()) {
        dequeued.push_back(std::move(opt.value()));
    }
    REQUIRE_THAT(dequeued, Catch::Matchers::Equals(expected_values));
}

TEMPLATE_TEST_CASE("MultithreadedProducerConsumerCorrectness", "[ThreadSafeQueue][concurrency]",
                   int, std::string) {
    ThreadSafeQueue<TestType> queue;
    constexpr size_t num_items = 10000;
    constexpr size_t num_producers = 4;
    constexpr size_t num_consumers = 4;
    std::atomic<size_t> consumed{0};

    std::vector<std::thread> producers, consumers;

    // Producers
    for (size_t p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, p, num_items]() {
            for (size_t i = 0; i < num_items; ++i) {
                if constexpr (std::is_same_v<TestType, int>) {
                    queue.enqueue(static_cast<int>(p * num_items + i));
                } else {
                    queue.enqueue(std::string("p") + std::to_string(p) + "_" + std::to_string(i));
                }
            }
        });
    }

    // Consumers
    for (size_t c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&queue, num_items, num_producers, &consumed]() {
            size_t local_count = 0;
            while (local_count < (num_items * num_producers) / num_consumers) {
                if (auto opt = queue.dequeue()) {
                    (void)opt.value();
                    ++local_count;
                    ++consumed;
                }
            }
        });
    }

    // Join producers first
    for (auto& t : producers)
        t.join();

    // Poll for consumer completion
    auto start = std::chrono::steady_clock::now();
    while (consumed < num_items * num_producers &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(consumed == num_items * num_producers);

    for (auto& t : consumers)
        t.join();
}

TEMPLATE_TEST_CASE("WaitAndDequeueProducerConsumer", "[ThreadSafeQueue][condition_variable]", int,
                   std::string) {
    ThreadSafeQueue<TestType> queue;
    std::vector<TestType> expected_values;
    if constexpr (std::is_same_v<TestType, int>) {
        expected_values = {1, 2, 3};
    } else {
        expected_values = {"one", "two", "three"};
    }
    std::vector<TestType> received;

    std::thread producer([&queue, &expected_values]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto& val : expected_values) {
            queue.enqueue(val);
        }
    });

    SECTION("Consumer waits and receives all") {
        for (size_t i = 0; i < expected_values.size(); ++i) {
            auto val = queue.wait_and_dequeue();
            received.push_back(std::move(val));
        }
    }

    producer.join();
    REQUIRE_THAT(received, Catch::Matchers::Equals(expected_values));
}

TEMPLATE_TEST_CASE("WaitAndDequeueMultithreadedNoDeadlock",
                   "[ThreadSafeQueue][condition_variable][concurrency]", int, std::string) {
    ThreadSafeQueue<TestType> queue;
    constexpr size_t num_items = 1000;
    constexpr size_t num_threads = 8;
    std::atomic<size_t> total_enqueued{0}, total_dequeued{0};

    std::vector<std::thread> threads;
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&queue, num_items, t, &total_enqueued, &total_dequeued]() {
            for (size_t i = 0; i < num_items / 2; ++i) {
                if constexpr (std::is_same_v<TestType, int>) {
                    queue.enqueue(static_cast<int>(t * 100 + i));
                } else {
                    queue.enqueue(std::string("t") + std::to_string(t) + "_" + std::to_string(i));
                }
                ++total_enqueued;
                if (i % 2 == 0) {
                    auto val = queue.wait_and_dequeue();
                    (void)val;
                    ++total_dequeued;
                }
            }
        });
    }

    for (auto& th : threads)
        th.join();
    REQUIRE(total_enqueued == num_items * num_threads / 2);
    INFO("Dequeued: " << total_dequeued.load());
}
