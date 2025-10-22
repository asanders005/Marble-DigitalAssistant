#include "RealtimeTranscriber.h"
#include <iostream>

RealtimeTranscriber::RealtimeTranscriber(PCMQueue &queue, Config config)
    : queue_(queue), cfg_(std::move(config)), resultThread_(nullptr), running_(false), pool_(nullptr)
{
}

RealtimeTranscriber::~RealtimeTranscriber() { stop(); }

bool RealtimeTranscriber::start()
{
    if (running_.load()) return false;

    pool_ = std::make_unique<InferencePool>(cfg_.modelPath, cfg_.threads, cfg_.chunkSeconds, 1, 4);
    if (!pool_->start()) {
        std::cerr << "RealtimeTranscriber: failed to start inference pool" << std::endl;
        return false;
    }

    running_.store(true);
    resultThread_.reset(new std::thread(&RealtimeTranscriber::resultLoop, this));
    return true;
}

void RealtimeTranscriber::stop()
{
    if (!running_.load()) return;
    running_.store(false);

    if (pool_) pool_->stop();

    if (resultThread_ && resultThread_->joinable()) resultThread_->join();
    resultThread_.reset();

    pool_.reset();
}

void RealtimeTranscriber::resultLoop()
{
    TranscriptionResult res;
    while (running_.load() && pool_ && pool_->popResult(res)) {
        if (!res.text.empty()) {
            std::cout << "[segment] " << res.text << std::endl;
        }
    }
}

