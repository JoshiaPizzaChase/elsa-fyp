#pragma once

#include <atomic>

/*
 * C++ does not provide a native implementation for a spinlock.
 */
class SpinlockMutex {
  private:
    std::atomic_flag flag{ATOMIC_FLAG_INIT};

  public:
    void get_lock() {
        while (!flag.test_and_set(std::memory_order_acquire)) {
            // spin until we successfully set the value atomically
        }
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};
