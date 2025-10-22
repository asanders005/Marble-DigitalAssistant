#pragma once

#include "BoundedQueue.h"
#include "InferenceWorker.h"
#include "PCMQueue.h"
#include <memory>
#include <string>
#include <vector>
#include <whisper.h>

class InferencePool {
public:
    InferencePool(const std::string& modelPath, int nWorkers, int chunkSeconds, int workerThreads, size_t queueCapacity = 4);
    ~InferencePool();

    bool start();
    void stop();

    // push captured PCM chunk into task queue (drop oldest if full)
    bool pushTask(const PCMQueue::Chunk& chunk);

    // pop transcription result; blocks until available or returns false if stopped
    bool popResult(TranscriptionResult& out);

private:
    std::string modelPath_;
    whisper_context* ctx_ = nullptr;
    int nWorkers_;
    int chunkSeconds_;
    int workerThreads_;

    BoundedQueue<PCMQueue::Chunk> taskQueue_;
    BoundedQueue<TranscriptionResult> resultQueue_;
    std::vector<std::unique_ptr<InferenceWorker>> workers_;
};
