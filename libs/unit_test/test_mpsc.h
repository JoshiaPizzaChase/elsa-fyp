#pragma once

#include "inter_process/mpsc_shared_memory_ring_buffer.h"
struct ComplexObject {
    int id;        // Trivial type
    char data[20]; // Fixed-size char array

    ComplexObject(int id, const char* str) : id(id) {
        std::strncpy(data, str, sizeof(data)); // Safe copy to ensure we don't overflow
        data[sizeof(data) - 1] = '\0';         // Null-terminate
    }
};

using Buffer = MpscSharedMemoryRingBuffer<ComplexObject, 2>;
