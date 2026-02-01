#ifndef CORE_THREAD_SAFE_QUEUE_H
#define CORE_THREAD_SAFE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>

namespace core {

/*
 * Standard LOCK-BASED thread-safe queue using standard library std::queue.
 * TODO: Exceptoin-unsafe if throwable copy/move constructors. Check the dequeue method.
 */
template <typename T>
class ThreadSafeQueue {
  public:
    void enqueue(T value) {
        std::lock_guard lock{m_mutex};
        m_queue.emplace(std::move(value));
        m_condition_var.notify_one();
    }

    // A copy or move occurs when returning value into std::optional<T>.
    // If that throws, we have lost the value due to m_queue.pop().
    // Alternatives:
    // - pass in a reference (assuming T is copy/move-assignable), or
    // - return a shared pointer, which won't throw.
    std::optional<T> dequeue() {
        std::lock_guard lock{m_mutex};
        if (m_queue.empty()) {
            return std::nullopt;
        }
        auto value{
            std::move(m_queue.front())}; // If this throws, we are fine as no popping has occured.
        m_queue.pop();
        return value;
    }

    // An optimized dequeuing method to replace busy-waiting.
    // Tries to dequeue the object only when the queue in non-empty, in a thread safe manner.
    // Using in a while-true loop is safe as condition variables sleeps the thread.
    T wait_and_dequeue() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_var.wait(lock, [this]() { return !m_queue.empty(); });
        auto value{std::move(m_queue.front())};
        m_queue.pop();
        return value;
    }

  private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_condition_var;
    std::queue<T> m_queue;
};

} // namespace core

#endif // CORE_THREAD_SAFE_QUEUE_H
