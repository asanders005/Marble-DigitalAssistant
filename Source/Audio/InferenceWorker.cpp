#include "InferenceWorker.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>

InferenceWorker::InferenceWorker(whisper_context* ctx,
                                 BoundedQueue<PCMQueue::Chunk>& tasks,
                                 BoundedQueue<TranscriptionResult>& results,
                                 int chunkSeconds,
                                 int workerThreads)
    : ctx_(ctx), tasks_(tasks), results_(results), chunkSeconds_(chunkSeconds), workerThreads_(workerThreads)
{
    const int sampleRate = WHISPER_SAMPLE_RATE;
    chunkSamples_ = std::max(1, chunkSeconds_ * sampleRate);
}

InferenceWorker::~InferenceWorker() {
    stop();
}

void InferenceWorker::start() {
    if (running_.load()) return;
    running_.store(true);
    // create worker state
    state_ = whisper_init_state(ctx_);
    thread_.reset(new std::thread(&InferenceWorker::run, this));
}

void InferenceWorker::stop() {
    if (!running_.load()) return;
    running_.store(false);
    tasks_.close();
    if (thread_ && thread_->joinable()) thread_->join();
    thread_.reset();
    if (state_) {
        whisper_free_state(state_);
        state_ = nullptr;
    }
}

void InferenceWorker::run() {
    std::vector<float> pcmf32;
    pcmf32.reserve(chunkSamples_);

    PCMQueue::Chunk chunk;
    while (running_.load() && tasks_.pop(chunk)) {
        // append
        for (int16_t s : chunk) pcmf32.push_back(s / 32768.0f);

        // process full chunks
        while ((int)pcmf32.size() >= chunkSamples_) {
            // call whisper_full_with_state
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            wparams.print_progress = false;
            wparams.print_special = false;
            wparams.translate = false;
            wparams.n_threads = workerThreads_;

            int ret = whisper_full_with_state(ctx_, state_, wparams, pcmf32.data(), chunkSamples_);
            if (ret != 0) {
                std::cerr << "InferenceWorker: whisper_full_with_state failed: " << ret << std::endl;
                break;
            }

            const int n_segments = whisper_full_n_segments(ctx_);
            std::string combined;
            for (int i = 0; i < n_segments; ++i) {
                const char * txt = whisper_full_get_segment_text(ctx_, i);
                if (txt) combined += txt;
            }

            TranscriptionResult r;
            r.timestamp_s = 0.0; // TODO: set proper timestamp if available
            r.text = std::move(combined);
            results_.push_drop_oldest(r);

            // drop processed samples
            pcmf32.erase(pcmf32.begin(), pcmf32.begin() + chunkSamples_);
        }
    }
}
