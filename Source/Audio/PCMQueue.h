#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
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
     * @error Throws std::runtime_error if the queue is closed.
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