#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

class PCMQueue
{
public:
    using Chunk = std::vector<int16_t>;

    // maxSize of 0 = unbounded queue
    explicit PCMQueue(size_t maxSize = 0) : maxSize(maxSize), closed(false) {}

    /**
     * Push a chunk of PCM data into the queue.
     * If the queue is full, this will block until space is available.
     */
    void Push(const Chunk& chunk);

    /**
     * Pop a chunk of PCM data from the queue.
     * If the queue is empty, this will block until data is available.
     * @return true if a chunk was popped, false if the queue is closed and empty.
     */
    bool Pop(Chunk& outChunk);

    /**
     * Try to pop a chunk of PCM data from the queue.
     * If the queue is empty, this will return immediately.
     * @return true if a chunk was popped, false if the queue is closed and empty.
     */
    bool TryPop(Chunk& outChunk);

    /**
     * Close the queue. After closing, no more data can be pushed,
     * and popping from an empty queue will return false.
     */
    void Close();

    size_t Size() const;
    bool Empty() const;
    void Clear();

private:
    mutable std::mutex mutex;
    std::condition_variable cvNotEmpty;
    std::condition_variable cvNotFull;
    std::deque<Chunk> queue;

    size_t maxSize;
    bool closed;
};

// Inline implementations
inline void PCMQueue::Push(const Chunk& chunk) {
    std::unique_lock lock(mutex);
    if (closed) std::cerr << "PCMQueue is closed" << std::endl;
    if (maxSize > 0) {
        cvNotFull.wait(lock, [&]{ return closed || queue.size() < maxSize; });
        if (closed) std::cerr << "PCMQueue is closed" << std::endl;
    }
    queue.emplace_back(chunk);
    cvNotEmpty.notify_one();
}

inline bool PCMQueue::Pop(Chunk& outChunk) {
    std::unique_lock lock(mutex);
    cvNotEmpty.wait(lock, [&]{ return closed || !queue.empty(); });
    if (queue.empty()) return false;
    outChunk = std::move(queue.front());
    queue.pop_front();
    if (maxSize > 0) cvNotFull.notify_one();
    return true;
}

inline bool PCMQueue::TryPop(Chunk& outChunk) {
    std::scoped_lock lock(mutex);
    if (queue.empty()) return false;
    outChunk = std::move(queue.front());
    queue.pop_front();
    if (maxSize > 0) cvNotFull.notify_one();
    return true;
}

inline void PCMQueue::Close() {
    {
        std::scoped_lock lock(mutex);
        closed = true;
    }
    cvNotEmpty.notify_all();
    cvNotFull.notify_all();
}

inline size_t PCMQueue::Size() const {
    std::scoped_lock lock(mutex);
    return queue.size();
}

inline bool PCMQueue::Empty() const {
    std::scoped_lock lock(mutex);
    return queue.empty();
}

inline void PCMQueue::Clear() {
    std::scoped_lock lock(mutex);
    queue.clear();
    if (maxSize > 0) cvNotFull.notify_all();
}