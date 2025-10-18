#include "PCMQueue.h"
#include <stdexcept>
#include <thread>

void PCMQueue::Push(const Chunk& chunk)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (closed) throw std::runtime_error("Cannot push to a closed PCMQueue");

    if (maxSize > 0)
    {
        cvNotFull.wait(lock, [this]() { return queue.size() < maxSize || closed; });
        if (closed) throw std::runtime_error("Cannot push to a closed PCMQueue");
    }
    queue.emplace_back(std::move(chunk));
    cvNotEmpty.notify_one();
}

bool PCMQueue::Pop(Chunk& outChunk)
{
    std::unique_lock<std::mutex> lock(mutex);
    cvNotEmpty.wait(lock, [this]() { return !queue.empty() || closed; });

    if (queue.empty() && closed) return false;

    outChunk = std::move(queue.front());
    queue.pop_front();
    if (maxSize > 0) cvNotFull.notify_one();
    return true;
}

bool PCMQueue::TryPop(Chunk& outChunk)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (queue.empty()) return false;

    outChunk = std::move(queue.front());
    queue.pop_front();
    if (maxSize > 0) cvNotFull.notify_one();
    return true;
}

void PCMQueue::Close()
{
    {
        std::scoped_lock lock(mutex);
        closed = true;
    }
    cvNotEmpty.notify_all();
    cvNotFull.notify_all();
}

size_t PCMQueue::Size() const
{
    std::scoped_lock lock(mutex);
    return queue.size();
}

bool PCMQueue::Empty() const
{
    std::scoped_lock lock(mutex);
    return queue.empty();
}

void PCMQueue::Clear()
{
    std::scoped_lock lock(mutex);
    queue.clear();
    if (maxSize > 0) cvNotFull.notify_all();
}