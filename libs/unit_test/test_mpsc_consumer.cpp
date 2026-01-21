#include "mpsc_shared_memory_ring_buffer.h" // Adjust the include path as per your project structure
#include "test.h"
#include <iostream>
#include <cassert>

int main() {
    Buffer ring_buffer =
            Buffer::create("/test_shm");
    while (true) {
        auto obj_ = ring_buffer.try_pop();
        if (obj_.has_value()) {
            auto obj = obj_.value();
            std::cout << "Consumed: id = " << obj.id << ", data = " << obj.data << std::endl;
            assert(obj.data[0] != '\0'); // Check that data is valid
        }
    }
}