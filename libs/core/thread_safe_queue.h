#ifndef CORE_THREAD_SAFE_QUEUE_H
#define CORE_THREAD_SAFE_QUEUE_H

#include <mutex>
#include <queue>

namespace core {

/*
 * Standard LOCK-BASED thread-safe queue using standard library std::queue.
 * TODO: Unsafe if throwable copy/move constructors. Check the dequeue method.
 */
template <typename T>
class ThreadSafeQueue {
  public:
    void enqueue(T message) {
        std::lock_guard lock{m_mutex};
        m_queue.emplace(std::move(message));
    }

    std::optional<T> dequeue() {
        std::lock_guard lock{m_mutex};
        if (m_queue.empty()) {
            return std::nullopt;
        }
        auto message{std::move(m_queue.front())};
        m_queue.pop();
        return message;
    }

  private:
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
};

} // namespace core

#endif // CORE_THREAD_SAFE_QUEUE_H
