#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity = 0) : capacity_(capacity), closed_(false) {}

    // push with drop-oldest policy when bounded and full
    bool push_drop_oldest(const T& item) {
        std::unique_lock lock(mutex_);
        if (closed_) return false;
        if (capacity_ > 0 && queue_.size() >= capacity_) {
            queue_.pop_front(); // drop oldest
        }
        queue_.push_back(item);
        cv_.notify_one();
        return true;
    }

    // blocking pop; returns false when closed and empty
    bool pop(T& out) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&]{ return closed_ || !queue_.empty(); });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void close() {
        {
            std::scoped_lock lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    size_t size() const {
        std::scoped_lock lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
    size_t capacity_;
    bool closed_;
};
