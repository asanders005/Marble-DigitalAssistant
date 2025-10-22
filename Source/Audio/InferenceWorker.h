#pragma once

#include "BoundedQueue.h"
#include "PCMQueue.h"
#include <whisper.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

struct TranscriptionResult {
    double timestamp_s = 0.0;
    std::string text;
};

class InferenceWorker {
public:
    InferenceWorker(whisper_context* ctx,
                    BoundedQueue<PCMQueue::Chunk>& tasks,
                    BoundedQueue<TranscriptionResult>& results,
                    int chunkSeconds,
                    int workerThreads);

    ~InferenceWorker();

    void start();
    void stop();

private:
    void run();

    whisper_context* ctx_ = nullptr;
    whisper_state* state_ = nullptr;
    BoundedQueue<PCMQueue::Chunk>& tasks_;
    BoundedQueue<TranscriptionResult>& results_;
    int chunkSeconds_;
    int chunkSamples_;
    int workerThreads_;

    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> running_ { false };
};
