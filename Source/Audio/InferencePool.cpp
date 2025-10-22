#include "InferencePool.h"
#include <iostream>
#include <whisper.h>

InferencePool::InferencePool(const std::string& modelPath, int nWorkers, int chunkSeconds, int workerThreads, size_t queueCapacity)
    : modelPath_(modelPath), nWorkers_(nWorkers), chunkSeconds_(chunkSeconds), workerThreads_(workerThreads), taskQueue_(queueCapacity), resultQueue_(queueCapacity)
{
}

InferencePool::~InferencePool() {
    stop();
}

bool InferencePool::start() {
    if (ctx_) return false;
    // initialize context
    ctx_ = whisper_init_from_file(modelPath_.c_str());
    if (!ctx_) {
        std::cerr << "InferencePool: failed to load model " << modelPath_ << std::endl;
        return false;
    }

    // create workers
    workers_.reserve(nWorkers_);
    for (int i = 0; i < nWorkers_; ++i) {
        workers_.emplace_back(std::make_unique<InferenceWorker>(ctx_, taskQueue_, resultQueue_, chunkSeconds_, workerThreads_));
        workers_.back()->start();
    }

    return true;
}

void InferencePool::stop() {
    // stop workers
    for (auto& w : workers_) {
        if (w) w->stop();
    }
    workers_.clear();

    taskQueue_.close();
    resultQueue_.close();

    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool InferencePool::pushTask(const PCMQueue::Chunk& chunk) {
    return taskQueue_.push_drop_oldest(chunk);
}

bool InferencePool::popResult(TranscriptionResult& out) {
    return resultQueue_.pop(out);
}
