#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

template<typename T, std::size_t POWER_OF_2_CAPACITY>
class MpscRingBuffer {
    static_assert((POWER_OF_2_CAPACITY & (POWER_OF_2_CAPACITY - 1)) == 0, "Buffer capacity must be a power of 2");
    static_assert(POWER_OF_2_CAPACITY > 0, "Negative buffer capacity");
    static_assert(std::is_trivially_copyable_v<T>, "T is not trivially copyable for shm");

private:
    static constexpr std::size_t mask = POWER_OF_2_CAPACITY - 1;
    static constexpr std::uint64_t magic_number = 0xDEADBEEF12345678ULL;

    struct Slot {
        std::atomic<std::size_t> sequence;
        T data;
    };

    // cache line padding
    static constexpr std::size_t cache_line_padding_size = 64;

    alignas(cache_line_padding_size) std::atomic<std::uint64_t> initialized_{0};
    alignas(cache_line_padding_size) std::atomic<std::size_t> head_;
    alignas(cache_line_padding_size) std::atomic<std::size_t> tail_;
    alignas(cache_line_padding_size) Slot slots_[POWER_OF_2_CAPACITY];

public:
    void init() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        for (std::size_t i = 0; i < POWER_OF_2_CAPACITY; ++i) {
            slots_[i].sequence.store(i, std::memory_order_relaxed);
        }
        initialized_.store(magic_number, std::memory_order_release);
    }

    void destroy() {
        initialized_.store(0, std::memory_order_release);
    }

    bool is_initialized() const {
        return initialized_.load(std::memory_order_acquire) == magic_number;
    }

    bool try_push(const T &value) {
        std::size_t position = head_.load(std::memory_order_relaxed);
        for (;;) {
            Slot &slot = slots_[position & mask];
            std::size_t seq = slot.sequence.load(std::memory_order_acquire);
            std::intptr_t difference = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(position);

            if (difference == 0) {
                // available slot for writing
                if (head_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                    slot.data = value;
                    slot.sequence.store(position + 1, std::memory_order_release);
                    return true;
                }
            } else if (difference < 0) {
                // buffer is full
                return false;
            } else {
                // another producer is faster, reload position
                position = head_.load(std::memory_order_relaxed);
            }
        }
    }

    void push_blocking(const T &value) {
        // spinning until successfully pushed
        while (!try_push(value)) {
        }
    }

    std::optional<T> try_pop() {
        std::size_t position = tail_.load(std::memory_order_relaxed);
        Slot &slot = slots_[position & mask];
        std::size_t seq = slot.sequence.load(std::memory_order_acquire);
        std::intptr_t difference = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(position + 1);

        if (difference < 0) {
            // buffer is empty
            return std::nullopt;
        }

        T result = slot.data;
        slot.sequence.store(position + POWER_OF_2_CAPACITY, std::memory_order_release);
        tail_.store(position + 1, std::memory_order_relaxed);
        return result;
    }


    bool empty() const {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return head == tail;
    }

    std::size_t size() const {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return head - tail;
    }

    static constexpr std::size_t capacity() { return POWER_OF_2_CAPACITY; }

    static constexpr std::size_t shm_size() { return sizeof(MpscRingBuffer); }
};

template<typename T, std::size_t POWER_OF_2_CAPACITY>
class MpscSharedMemoryRingBuffer {
    using Buffer = MpscRingBuffer<T, POWER_OF_2_CAPACITY>;

private:
    MpscSharedMemoryRingBuffer() = default;

    Buffer *buffer_ = nullptr;
    bool is_owner_ = false;
    std::string shm_file_path_;
    bool use_shm_open_ = true;

public:
    static std::string get_shm_file_full_name(const std::string &shm_file_name_prefix) {
        return shm_file_name_prefix + "_" + std::to_string(Buffer::shm_size()) + "_" + std::to_string(POWER_OF_2_CAPACITY);
    }

    static MpscSharedMemoryRingBuffer create(const std::string &shm_file_name_prefix, bool use_shm_open = true) {
        MpscSharedMemoryRingBuffer buf;
        // creator is the owner, who will be responsible for destroying the physical shared memory
        // in our use case consumer is the owner of the shm
        buf.is_owner_ = true;
        buf.shm_file_path_ = get_shm_file_full_name(shm_file_name_prefix);
        buf.use_shm_open_ = use_shm_open;

        int fd;
        if (use_shm_open) {
            fd = shm_open(buf.shm_file_path_.c_str(), O_CREAT | O_RDWR, 0666);
        } else {
            fd = ::open(buf.shm_file_path_.c_str(), O_CREAT | O_RDWR, 0666);
        }

        if (fd == -1) {
            throw std::runtime_error("Failed to create shm: " + std::string(strerror(errno)));
        }

        if (ftruncate(fd, Buffer::shm_size()) == -1) {
            close(fd);
            throw std::runtime_error("Failed to create shm: " + std::string(strerror(errno)));
        }

        void *ptr = mmap(nullptr, Buffer::shm_size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to mmap shm: " + std::string(strerror(errno)));
        }

        buf.buffer_ = static_cast<Buffer *>(ptr);
        if (!buf.buffer_->is_initialized()) {
            buf.buffer_->init();
        }

        return buf;
    }

    static MpscSharedMemoryRingBuffer
    open_exist_shm(const std::string &shm_file_name_prefix, bool use_shm_open = true) {
        MpscSharedMemoryRingBuffer buf;
        buf.is_owner_ = false;
        buf.shm_file_path_ = get_shm_file_full_name(shm_file_name_prefix);
        buf.use_shm_open_ = use_shm_open;

        int fd;
        if (use_shm_open) {
            fd = shm_open(buf.shm_file_path_.c_str(), O_RDWR, 0666);
        } else {
            fd = ::open(buf.shm_file_path_.c_str(), O_RDWR, 0666);
        }

        if (fd == -1) {
            throw std::runtime_error("Failed to open shm: " + std::string(strerror(errno)));
        }

        void *ptr = mmap(nullptr, Buffer::shm_size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to mmap shm: " + std::string(strerror(errno)));
        }

        buf.buffer_ = static_cast<Buffer *>(ptr);
        if (!buf.buffer_->is_initialized()) {
            buf.buffer_->init();
        }

        return buf;
    }

    void push_blocking(T &value) {
        buffer_->push_blocking(value);
    }

    bool try_push(T &value) {
        return buffer_->try_push(value);
    }

    std::optional<T> try_pop() {
        return buffer_->try_pop();
    }

    ~MpscSharedMemoryRingBuffer() {
        if (buffer_) {
            if (is_owner_) {
                buffer_->destroy();
            }
            munmap(buffer_, Buffer::shm_size());
            if (is_owner_) {
                if (use_shm_open_) {
                    shm_unlink(shm_file_path_.c_str());
                } else {
                    unlink(shm_file_path_.c_str());
                }
            }
        }
    }

    // move constructor
    MpscSharedMemoryRingBuffer(MpscSharedMemoryRingBuffer &&other) noexcept
        : buffer_(other.buffer_),
          is_owner_(other.is_owner_),
          shm_file_path_(other.shm_file_path_) {
        other.buffer_ = nullptr;
    }

    MpscSharedMemoryRingBuffer &operator=(MpscSharedMemoryRingBuffer &&other) noexcept {
        if (this != &other) {
            if (buffer_) {
                if (is_owner_) {
                    buffer_->destroy();
                }
                munmap(buffer_, Buffer::shm_size());
                if (is_owner_) {
                    if (use_shm_open_) {
                        shm_unlink(shm_file_path_.c_str());
                    } else {
                        unlink(shm_file_path_.c_str());
                    }
                }
            }
            buffer_ = other.buffer_;
            is_owner_ = other.is_owner_;
            shm_file_path_ = other.shm_file_path_;
            use_shm_open_ = other.use_shm_open_;
            other.buffer_ = nullptr;
        }
        return *this;
    }

    MpscSharedMemoryRingBuffer(const MpscSharedMemoryRingBuffer &) = delete;

    MpscSharedMemoryRingBuffer &operator=(const MpscSharedMemoryRingBuffer &) = delete;

    Buffer *operator->() { return buffer_; }
    const Buffer *operator->() const { return buffer_; }
    Buffer &operator*() { return *buffer_; }
    const Buffer &operator*() const { return *buffer_; }
    Buffer *get() { return buffer_; }
    const Buffer *get() const { return buffer_; }
};
