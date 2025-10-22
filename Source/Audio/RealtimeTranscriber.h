#pragma once

#include "PCMQueue.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include "InferencePool.h"

// Forward-declare whisper types to avoid including the header in the public API.
extern "C" {
    struct whisper_context;
}

class RealtimeTranscriber {
public:
    struct Config {
        std::string modelPath;
        int threads = 1;
        int chunkSeconds = 1; // chunk length in seconds
    };

    RealtimeTranscriber(PCMQueue &queue, Config config);
    ~RealtimeTranscriber();

    bool start();
    void stop();

private:
    void resultLoop();

    PCMQueue &queue_;
    Config cfg_;
    std::unique_ptr<std::thread> resultThread_;
    std::atomic<bool> running_{false};

    // inference pool
    std::unique_ptr<InferencePool> pool_;
};
