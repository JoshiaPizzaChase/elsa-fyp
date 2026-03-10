#include "inter_process/mpsc_shared_memory_ring_buffer.h"
#include "test_mpsc.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

constexpr int num_producers = 3;
constexpr int data_per_producer = 5;

void producer(Buffer& ring_buffer, int producer_id) {
    for (int i = 0; i < data_per_producer; ++i) {
        ComplexObject obj(
            i + producer_id * 10,
            ("Producer " + std::to_string(producer_id) + ", Data " + std::to_string(i)).c_str());
        ring_buffer.push_blocking(obj);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulates work
    }
}

int main() {
    try {
        // Create shared memory ring buffer
        Buffer ring_buffer = Buffer::open_exist_shm("/test_shm");

        std::vector<std::thread> producers;

        // Start multiple producer threads
        for (int i = 0; i < num_producers; ++i) {
            producers.emplace_back(producer, std::ref(ring_buffer), i);
        }

        // Join producer threads
        for (auto& prod_thread : producers) {
            prod_thread.join();
        }

        std::cout << "All messages transferred successfully!" << std::endl;

        // Cleanup will happen in the destructor of ring_buffer
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}