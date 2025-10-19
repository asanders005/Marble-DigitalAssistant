#include "RealtimeTranscriber.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

// include whisper API
#include <whisper.h>

RealtimeTranscriber::RealtimeTranscriber(PCMQueue &queue, Config config)
    : queue_(queue), cfg_(std::move(config)), worker_(nullptr), running_(false), ctx_(nullptr)
{
}

RealtimeTranscriber::~RealtimeTranscriber() {
    stop();
}

bool RealtimeTranscriber::start() {
    if (running_.load()) return false;

    // init whisper
    // Initialize context using default params. Thread count is set on the full params below.
    ctx_ = whisper_init_from_file(cfg_.modelPath.c_str());
    if (!ctx_) {
        std::cerr << "Failed to initialize Whisper model from " << cfg_.modelPath << std::endl;
        return false;
    }

    running_.store(true);
    worker_.reset(new std::thread(&RealtimeTranscriber::workerLoop, this));
    return true;
}

void RealtimeTranscriber::stop() {
    if (!running_.load()) return;
    running_.store(false);
    queue_.Close();
    if (worker_ && worker_->joinable()) worker_->join();
    worker_.reset();
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

void RealtimeTranscriber::workerLoop() {
    PCMQueue::Chunk chunk;

    // compute expected samples per chunk
    const int sampleRate = WHISPER_SAMPLE_RATE; // 16000
    const int chunkSamples = cfg_.chunkSeconds * sampleRate;
    const int minSamples = std::max(1, sampleRate / 10); // 100 ms minimum

    std::vector<float> pcm_accum;
    pcm_accum.reserve(chunkSamples);

    while (running_.load() && queue_.Pop(chunk)) {
        // append int16 samples converted to float in [-1,1]
        pcm_accum.reserve(pcm_accum.size() + chunk.size());
        for (int16_t s : chunk) pcm_accum.push_back(s / 32768.0f);

        // only run inference when we have at least one full chunk
        while ((int)pcm_accum.size() >= chunkSamples) {
            // run inference on the first chunkSamples
            whisper_full_params fparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            fparams.print_special = false;
            fparams.print_progress = false;
            fparams.translate = false;
            fparams.n_threads = cfg_.threads;

            int ret = whisper_full(ctx_, fparams, pcm_accum.data(), chunkSamples);
            if (ret != 0) {
                std::cerr << "whisper_full failed: " << ret << std::endl;
                break;
            }

            const int n_segments = whisper_full_n_segments(ctx_);
            for (int i = 0; i < n_segments; ++i) {
                const char * text = whisper_full_get_segment_text(ctx_, i);
                std::cout << "[segment] " << text << std::endl;
            }

            // remove processed samples
            pcm_accum.erase(pcm_accum.begin(), pcm_accum.begin() + chunkSamples);
        }
    }

    // drain remaining: if there's remaining audio when stopping or queue closed,
    // process it if it's long enough by padding to a full chunk if necessary.
    if (!pcm_accum.empty()) {
        if ((int)pcm_accum.size() >= minSamples) {
            // pad to chunkSamples if shorter
            if ((int)pcm_accum.size() < chunkSamples) {
                pcm_accum.resize(chunkSamples, 0.0f);
            }

            whisper_full_params fparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            fparams.print_special = false;
            fparams.print_progress = false;
            fparams.translate = false;
            fparams.n_threads = cfg_.threads;

            int ret = whisper_full(ctx_, fparams, pcm_accum.data(), (int)pcm_accum.size());
            if (ret == 0) {
                const int n_segments = whisper_full_n_segments(ctx_);
                for (int i = 0; i < n_segments; ++i) {
                    const char * text = whisper_full_get_segment_text(ctx_, i);
                    std::cout << "[segment] " << text << std::endl;
                }
            } else {
                std::cerr << "whisper_full failed on drain: " << ret << std::endl;
            }
        }
    }
}
